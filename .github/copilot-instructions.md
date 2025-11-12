# Copilot Instructions for toyssd

**toyssd** is a SystemC-based SSD simulator (C++20 + Python 3.13+). It uses CMake + Invoke tasks, GoogleTest + pytest, and runs CI on macOS (native) and Linux (Docker). Key dependency: SystemC 3.0.2.

## CRITICAL: Read [AGENTS.md](../AGENTS.md) First

**ALWAYS read AGENTS.md before making changes.** It contains detailed rules on code style, SystemC patterns, documentation requirements, and commit workflow.

## Build & Test Workflow

### Platform-Specific Commands

**macOS (native):**
```bash
uv run invoke bootstrap  # First time: installs deps, configures CMake
uv run invoke build      # Compiles C++, ~30-90s (incremental ~10s)
uv run invoke test       # Runs 31 C++ tests + Python tests, <5s
uv run invoke format     # Auto-format C++ and Python
uv run invoke lint       # Run all linters, ~30-120s
uv run invoke coverage   # Generate coverage reports, ~60-90s (90% C++, 60% Python thresholds)
```

**Linux/Windows (Docker):**
```bash
docker build --target dev -t toyssd-dev .
docker run --rm -it -v "$PWD":/workspaces/toyssd -w /workspaces/toyssd toyssd-dev bash
# Inside container:
invoke bootstrap && invoke build && invoke test && invoke lint
```

**Ubuntu 24.04 (Codex/CI, no Docker):**
```bash
./run/setup.sh                      # First time: installs toolchain + SystemC to /opt/systemc
source /etc/profile.d/toyssd.sh     # Or restart shell to pick up env vars
uv run invoke bootstrap             # Then proceed as macOS
```

### Standard Pre-Commit Checklist

Run in order before committing:
1. `uv run invoke format` - Apply clang-format (C++) and ruff (Python)
2. `uv run invoke lint` - Check clang-tidy, cpplint, ruff, pymarkdown (expect "All checks passed!")
3. `uv run invoke build` - Compile (expect some -Wshorten-64-to-32 warnings in host.cpp - normal)
4. `uv run invoke test` - Run tests (expect "100% tests passed, 0 tests failed out of 31")
5. `uv run invoke coverage` - If code coverage affected (must meet 90% C++, 60% Python)

### Troubleshooting

- **`invoke: command not found`** → Use `uv run invoke`
- **CMake can't find SystemC (Linux)** → Run `./run/setup.sh` or use Docker
- **Build fails "no SDK found" (macOS)** → Install Xcode Command Line Tools: `xcode-select --install`
- **Lint fails "compile_commands.json not found"** → Run `invoke build` first
- **Tests fail after changes** → Run `invoke clean` then `invoke bootstrap` to rebuild from scratch

## Project Layout (Key Files)

```
├── CMakeLists.txt              # C++20, SystemC, GoogleTest config
├── pyproject.toml              # Python deps, tool configs
├── tasks.py                    # Invoke entrypoint
├── tools/invoke_tasks.py       # All task definitions (bootstrap, build, test, etc.)
├── run/{setup.sh, maintenance_setup.sh}  # Codex/Ubuntu setup scripts
│
├── include/toyssd/*.hpp        # Public C++ headers (host, controller, nand, geometry, extensions)
├── src/**/*.cpp                # C++ implementations (organized by module)
├── python/toyssd/*.py          # Python package (sim, config, workload)
├── tests/*.cc                  # C++ unit tests (31 GoogleTests)
├── python/toyssd/tests/*.py    # Python unit tests (pytest)
├── examples/systemc_hello.cpp  # SystemC smoke test
│
├── docs/*.md                   # Architecture, Docker, coverage design
├── AGENTS.md                   # **CRITICAL** - AI agent rules (read first!)
├── CONTRIBUTING.md             # Contribution guidelines
├── README.md                   # Getting started
│
├── .github/workflows/ci.yml    # Main CI (macOS + Docker + coverage)
├── .clang-{format,tidy}        # C++ style/lint config
├── .pymarkdown.json            # Markdown lint config
└── Dockerfile                  # Multi-stage (base, dev, builder, runtime)
```

**Key files to update when:**
- **Adding C++ module:** `include/toyssd/*.hpp`, `src/*/*.cpp`, `CMakeLists.txt`, `tests/*.cc`, `docs/ssd_sim_design.md`
- **Adding Python module:** `python/toyssd/*.py`, `python/toyssd/tests/*.py`
- **Changing build:** Update `CMakeLists.txt`, `tools/invoke_tasks.py`, `Dockerfile`, `run/setup.sh` (keep in sync!)
- **Updating docs:** Use relative Markdown links, update AGENTS.md §10 Docs Index

## Code Style Quick Reference

**C++ (Google Style):**
- Types: `CamelCase`, Functions/vars: `snake_case`, Members: `snake_case_` (trailing underscore)
- Pointers: Left-aligned (`int* ptr`)
- Prefer plain C++ class form over `SC_MODULE`/`SC_CTOR` macros
- Use `tlm_utils::simple_*_socket` for TLM sockets
- Always honor TLM delays: `sc_core::wait(delay)` and check `payload.get_response_status()`
- See AGENTS.md §7 for full SystemC playbook

**Python (Ruff + Google):**
- Functions/vars: `snake_case`, Classes: `CamelCase`, Constants: `UPPER_SNAKE_CASE`
- Use dataclasses and type hints for public APIs

**Linters:** clang-tidy (Google/performance/readability), cpplint, ruff, pymarkdownlnt. Run via `invoke lint`.

## Known Issues (Expected)

1. **SC_HAS_PROCESS deprecation warning** - IEEE 1666-2023 deprecation, still functional
2. **Four -Wshorten-64-to-32 warnings in host.cpp** - size_t → uint32_t for TLM, acceptable
3. **Unused page_size_bytes_ in nand.hpp** - Reserved for future use
4. **Many suppressed warnings during clang-tidy** - From SystemC/GoogleTest headers, normal

## CI Pipeline Summary

- **macOS Native:** Apple Clang, native builds, ~5-8 min
- **Docker Dev:** Ubuntu + Clang, container parity check, ~10-15 min
- **Docker Coverage:** 90% C++, 60% Python thresholds, uploads reports, ~3-5 min
- **Codex Scripts:** Validates setup.sh/maintenance_setup.sh, ~15-20 min

## Final Checklist

Before any commit:
1. Read [AGENTS.md](../AGENTS.md) for repository rules
2. Use `uv run invoke` on macOS or Docker on Linux/Windows
3. Run: format → lint → build → test → coverage (if needed)
4. Update docs with relative Markdown links
5. Add tests to maintain coverage thresholds (90% C++, 60% Python)
