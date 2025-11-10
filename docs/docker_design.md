# toyssd — Docker Architecture Design Document

## 1. Overview

This document defines the Docker-based development and runtime architecture for the **toyssd** project—a Python + C++ (SystemC) SSD simulator. The container design ensures reproducible builds, consistent developer experience across Linux and macOS, and CI parity with local testing.

## 2. Goals

- Provide a unified Docker environment for development, CI, and runtime use.
- Simplify dependency management for Python (via `uv`) and C++ (via CMake + FetchContent).
- Enable Dev Container support for VSCode.
- Support lightweight runtime images for executing simulations.

## 3. Image Structure

**Multi-stage Dockerfile design:**

| Stage     | Purpose                      | Includes                                                                 |
| --------- | ---------------------------- | ------------------------------------------------------------------------ |
| `base`    | Foundation image             | OS packages, Python 3.14, LLVM/Clang toolchain, official `uv` binary     |
| `dev`     | Full development environment | `/opt/systemc`, PySysC headers, CMake/Ninja, Invoke tasks, VS Code hooks |
| `runtime` | Slim image for end-users     | Python runtime + toyssd wheel/stub backend + SystemC runtime libraries   |

## 4. Python Dependency Management in Docker

### 4.1 Dependency Strategy

Within Docker, Python environments are isolated and immutable once the image is built. The container itself defines both the Python version and dependency set, ensuring reproducibility and consistency across development, CI, and runtime contexts.

### 4.2 Role of `uv`

- `uv` remains part of the Docker build process to ensure **fast and deterministic installs**.
- It replaces `pip` for dependency resolution during **image build**, but is **not intended for use inside a running container** beyond the bootstrap commands.
- Developers **do not** create or switch environments inside the container; Docker provides the necessary isolation.

**Installation Pattern (build stage only):**

```Dockerfile
# Install uv once per image using the upstream installer.
RUN curl -LsSf https://astral.sh/uv/install.sh | env UV_INSTALL_DIR=/usr/local/bin sh

# Install the toyssd package (with dev extras) straight into the system interpreter
# so bind-mounted sources remain editable.
COPY pyproject.toml ./
COPY python ./python
RUN uv pip install --system --editable ./python[dev] \
 && rm -rf /tmp/toyssd /root/.cache
```

This keeps the repository bind-mounted and editable while still ensuring every container resolves dependencies through uv. GitHub Actions also uploads the resolved dependency list (`pip freeze` output) as an artifact for auditability.

**Rationale:**

| Environment              | Use `uv`? | Reason                                                                               |
| ------------------------ | --------- | ------------------------------------------------------------------------------------ |
| Host (outside Docker)    | ✅         | Environment sync and dependency management                                           |
| Docker build             | ✅         | Fast, reproducible dependency installation + editable mounts                         |
| Inside running container | ❌         | System interpreter already has deps; run `invoke`/`python` directly for simplicity   |

**Benefit:** This pattern keeps builds reproducible, ensures VS Code Dev Containers and GitHub Actions share dependency resolution logic, and avoids baking multiple copies of the source tree into the image.

## 5. Development Environment

The `dev` image provides the complete toolchain and aligns with the checked-in [`.devcontainer/devcontainer.json`](../.devcontainer/devcontainer.json) so VS Code can open the repo “in container” with zero configuration.

- SystemC is built once during the image build and installed under `/opt/systemc`. The container sets `TOYSSD_FETCH_SYSTEMC=OFF` and `TOYSSD_SYSTEMC_PREFIX=/opt/systemc`, letting CMake skip `FetchContent` inside Docker while local macOS/Linux builds still download SystemC automatically. We force the SystemC build to use C++20 so the exported `sc_api_version_*` symbol matches our project’s `CMAKE_CXX_STANDARD`, avoiding linker errors when the example/Unit tests link against the prebuilt library.
- PySysC headers are staged alongside SystemC so both the C++ tests and the future Python bindings resolve cleanly.
- Tooling: LLVM/Clang, `clang-tidy`, `clang-format`, `cpplint`, Ninja, CMake, `ccache`, `uv`, Invoke, pytest, Ruff.
- The devcontainer mounts a named `ccache` volume (`toyssd-ccache`) to persist compilation artifacts between sessions.
- [`.devcontainer/post-create.sh`](../.devcontainer/post-create.sh) reruns the SystemC/uv installers **only as a validation step** (they should already exist in the published image) and finishes with `invoke bootstrap` so ad‑hoc image rebuilds still land in a known-good state.
- Developers interact via VS Code or plain CLI with the `toyssd` user; the workspace is mounted at `/workspaces/toyssd`.

## 6. CI/CD Integration

GitHub Actions runs CI jobs using the same `toyssd-dev` container to ensure identical environments.

- Docker layers are cached using `cache-from` and `cache-to` for faster builds.
- Build artifacts (`ccache`, CMake build outputs) are cached to avoid redundant compilation.
- Optionally, the `runtime` image is published to GHCR for users who only need to run simulations.
- Dependency manifest artifact: each CI job runs `pip freeze` (via `uv pip freeze` for macOS and `pip freeze` inside the Docker dev image) and uploads the results so we can trace exactly which versions were used.
- **Optional SBOM artifact:** produce an SBOM for the built image and upload it as a CI artifact or echo it to logs for provenance, e.g.:

```yaml
- name: SBOM (debug/reproducibility)
  run: |
    docker sbom ghcr.io/org/toyssd:${{ github.sha }} --format cyclonedx-json > sbom.json
- name: Upload SBOM
  uses: actions/upload-artifact@v4
  with:
    name: sbom
    path: sbom.json
```

## 7. Runtime Image

The runtime stage is meant for users who want to run workloads without the full toolchain. Today the Python in-memory bridge is still the active backend, so the runtime image repackages:

- The toyssd wheel (Python orchestrator + stub backend)
- SystemC runtime libraries (ready for the future SystemC backend)
- Python runtime and CLI entrypoints

**Usage Modes (current state):**

1. **Python script:** mount a workload script inside the container and run it with the system `python` (dependencies are already installed into the base image), e.g.:

   ```bash
   docker run --rm -v "$PWD":/work ghcr.io/org/toyssd:runtime \
     python scripts/run_workload.py configs/sequential_small.py
   ```

2. **Interactive REPL:** this is simply launching `python` inside the container.

## 8. File Permissions and Volumes

- Non-root `toyssd` user inside the container for safe development.
- Optional named Docker volume (`toyssd-ccache`) for `ccache`. It is not required for correctness, but when attached it speeds up rebuilds automatically because CMake enables `ccache` if present.
- Workspace mount:
  - Dev containers: bind mount the repo at `/workspaces/toyssd` with `:cached` (macOS) to keep live-edit workflows smooth.
  - Runtime containers: mount workload/config files read-only as needed.

## 9. Platform Considerations

- Multi-arch builds (`linux/amd64`, `linux/arm64`) use Buildx and bake the correct SystemC variant into `/opt/systemc`.
- Local macOS/Linux developers continue to rely on CMake `FetchContent` for SystemC so they do not need Docker to iterate. The new CMake options (`TOYSSD_FETCH_SYSTEMC`, `TOYSSD_SYSTEMC_PREFIX`) let Docker reuse the preinstalled copy while hosts fetch sources on demand.
- Ninja is the default generator inside containers; `ccache` integration is automatic when the binary exists.

## 10. Reproducibility & Transparency

- Pin base image and all dependency versions as appropriate.
- Strip package manager caches after installs.
- Maintain checksum verification for SystemC and PySysC downloads.
- **Optional SBOM for debugging and reproducibility:** generate a Software Bill of Materials (e.g., via `docker sbom`) during CI and upload it as an artifact or include it in logs to document exactly what went into the image.

## 11. Developer Workflow Summary

```bash
# Inside VSCode or dev container
invoke build    # C++ build
invoke test     # Run GTest + pytest (stub backend + SystemC tests)
invoke lint     # Run clang-tidy/cpplint/Ruff
```

Runtime users who prefer to drive the simulator manually can open an interactive REPL and import `ToySSD`:

```bash
docker run --rm -it ghcr.io/org/toyssd:runtime python
```

Inside the REPL:

```python
from toyssd import ToySSD, SimConfig, NandGeometry
sim = ToySSD(SimConfig(nand_geometry=NandGeometry()))
p = sim.run_workload(...)  # etc.
```

Future enhancement: add a convenience console script (`toyssd run --config my_workload.json`) plus JSON config support so end users can avoid writing Python shims.

## 12. Future Enhancements

- CI pipeline to publish tagged Docker images per release.
- Potential switch to distroless runtime image for minimal footprint.

---
