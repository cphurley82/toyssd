# AGENTS.md

> **Purpose**  
> This file instructs AI coding agents (ChatGPT, Copilot, Codex, etc.) how to work in this repository.  
> It enforces consistent formatting/linting, up-to-date docs with valid links, correct SystemC usage, reproducible builds, and prevents unauthorized commits.

---

## 1) Project snapshot (machine-readable)

- **Domain:** SSD simulator using **SystemC** (host → controller → NAND)  
- **Build system:** CMake (C++20), examples + tests via CTest/GoogleTest  
- **Python tasks:** Invoke task runner (see §4)  
- **Container:** Docker image for non-macOS workflows & CI parity  
- **Docs of record:** [README.md](./README.md), [docs/ssd_sim_design.md](./docs/ssd_sim_design.md), [docs/docker_design.md](./docs/docker_design.md), [CONTRIBUTING.md](./CONTRIBUTING.md), [AGENTS.md](./AGENTS.md)

---

## 2) Agent contract (what you MUST do)

1. **Read first:** this file + [README.md](./README.md).  
2. **When you change code:**  
   - Run **format + lint**.  
   - **Build** and **test**.  
   - Update docs and ensure **every referenced repo file is a relative Markdown link**.  
3. **When you change docs:** keep links up to date and update the **Docs Index** (§10).  
4. Prefer **minimal diffs** (don’t reformat unrelated files).  
5. Do **not** change version pins, CMake policies, or Docker base images without explaining **why** and updating docs.  
6. **Never commit or push code unless explicitly told to do so.**

---

## 3) Platform routing (how to run tasks)

- **macOS:** run tasks **natively** (no Docker).  
- **Linux/Windows/CI:** run tasks **inside Docker** (see §5).

Decision rule:

```
if system == "Darwin":
    use: uv run invoke <task>
else:
    use: docker run … invoke <task>
```

---

## 4) Invoke tasks (entrypoint for bootstrap/build/test/lint)

```bash
invoke bootstrap  # Install Python deps and configure an initial Debug CMake build tree.
invoke format     # Auto-format C++/Python
invoke lint       # Run all linters
invoke build      # Configure & build project (CMake)
invoke test       # Run tests and examples
```

> Ensure these tasks exist in [invoke_tasks.py](./tools/invoke_tasks.py) or [tasks.py](./tasks.py).

---

## 5) Docker workflow (non-macOS & CI)

```bash
# Build the development image (toolchain + SystemC + dev tools)
docker build --target dev -t toyssd-dev .

# Enter the container with your workspace mounted at /workspaces/toyssd
docker run --rm -it \
  -v "$PWD":/workspaces/toyssd \
  -w /workspaces/toyssd \
  toyssd-dev bash

# Inside the container
invoke bootstrap
invoke format
invoke lint
invoke build
invoke test
```

- Use Docker to guarantee reproducible builds and CI parity.
- After rebuilding the Docker dev image or reopening a VS Code Dev Container, rerun `invoke bootstrap` to reconfigure the build tree.

---

## 6) Code style, formatting & linting

- **C++ / SystemC:** `clang-format`, `clang-tidy`, `cpplint`, and strict compiler warnings  
- **Python:** `ruff` (formatter + linter; configured in [pyproject.toml](./pyproject.toml))  

---

## 7) SystemC playbook

```cpp
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

class EchoTarget : public sc_core::sc_module {
 public:
  tlm_utils::simple_target_socket<EchoTarget> socket;

  explicit EchoTarget(const sc_core::sc_module_name& name)
      : sc_core::sc_module(name), socket("socket") {
    socket.register_b_transport(this, &EchoTarget::b_transport);
  }

 private:
  void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
    sc_core::wait(delay);  // Honor annotated delay.
    const auto* data = trans.get_data_ptr();
    std::cout << "EchoTarget saw cmd=" << trans.get_command()
              << " data=" << static_cast<int>(data[0]) << '\n';
    delay = sc_core::SC_ZERO_TIME;
  }
};

class PingInitiator : public sc_core::sc_module {
 public:
  tlm_utils::simple_initiator_socket<PingInitiator> socket;

  explicit PingInitiator(const sc_core::sc_module_name& name)
      : sc_core::sc_module(name), socket("socket") {
    SC_THREAD(Run);
  }

 private:
  void Run() {
    unsigned char data = 0x42;
    tlm::tlm_generic_payload payload;
    payload.set_address(0x100);
    payload.set_data_ptr(&data);
    payload.set_data_length(1);
    payload.set_command(tlm::TLM_WRITE_COMMAND);

    sc_core::sc_time delay = sc_core::sc_time(10, sc_core::SC_NS);
    socket->b_transport(payload, delay);
    if (payload.get_response_status() != tlm::TLM_OK_RESPONSE) {
      SC_REPORT_ERROR("PingInitiator", "target responded with error");
      return;
    }
    sc_core::wait(delay);  // Advance simulation for target latency.
  }
};

int sc_main(int argc, char* argv[]) {
  PingInitiator initiator("initiator");
  EchoTarget target("target");
  initiator.socket.bind(target.socket);
  sc_core::sc_start(sc_core::sc_time(100, sc_core::SC_NS));
  return 0;
}
```

Prefer the plain C++ class form (`class Foo : public sc_core::sc_module`) and avoid `SC_MODULE`/`SC_CTOR` macros. Use TLM-2.0 sockets (e.g., `tlm_utils::simple_*_socket`) and blocking transport helpers unless there is a strong reason to fall back to the raw `tlm::tlm_*_socket` API.

Avoid incorrect socket binding, or mixing `SC_METHOD` with `wait()` (use `SC_THREAD`/`SC_CTHREAD` when blocking).

- Always check `payload.get_response_status()` and honor annotated delays (`wait(delay)` when non‑zero).
- Manage TLM extension ownership: attach with `set_extension(...)`, then `release_extension(ptr)` and `delete` to avoid leaks.

---

## 8) Documentation policy

- Update all affected docs when code changes.  
- Convert all file mentions into relative Markdown links.  

---

## 9) Commits, PRs, review

**Write operations are opt-in.** Agents commit only when explicitly instructed.  
When authorized:

- Create a **topic branch** (never commit to `main`).  
- Open a **draft PR** unless “ready for review” is specified.  
- Confirm you ran: `invoke format` → `invoke lint` → `invoke build` → `invoke test`.  
- Use Conventional Commits style.

---

## 10) Docs index

| Area | File | Purpose |
|---|---|---|
| Overview | [README.md](./README.md) | Project overview |
| SystemC design | [ssd_sim_design.md](./docs/ssd_sim_design.md) | Architecture, timing, examples |
| Docker design | [docker_design.md](./docs/docker_design.md) | Container and CI design |
| Contributing | [CONTRIBUTING.md](./CONTRIBUTING.md) | Review, CI gates, style |
| Agent rules | [AGENTS.md](./AGENTS.md) | AI agent behavior |

---

## 11) Quickstart for agents

**Self-check (quick sanity):**

- Verify root has [CMakeLists.txt](./CMakeLists.txt) and [tools/invoke_tasks.py](./tools/invoke_tasks.py) (tasks registered via [tasks.py](./tasks.py)).
- List tasks with `invoke --list` to ensure `bootstrap`, `build`, `test`, `format`, `lint` exist.

**macOS (native):**

```bash
uv run invoke bootstrap
uv run invoke format
uv run invoke lint
uv run invoke build
uv run invoke test
```

**Linux/Windows/CI (Docker):**  

```bash
docker build --target dev -t toyssd-dev .
docker run --rm -it -v "$PWD":/workspaces/toyssd -w /workspaces/toyssd toyssd-dev bash
invoke bootstrap
invoke format
invoke lint
invoke build
invoke test
```

---

## 12) Maintainers’ notes

- **Invoke is the single public interface** for agents. Keep task names stable.  
- Keep this file aligned with [invoke_tasks.py](./tools/invoke_tasks.py), [pyproject.toml](./pyproject.toml), [Dockerfile](./Dockerfile), and [CMakeLists.txt](./CMakeLists.txt).  
- **Branch protection:** disallow direct pushes to `main`; require reviews + checks.
