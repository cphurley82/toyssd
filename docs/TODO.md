# Backlog / TODOs

## Publish Docker image to GHCR via CI

Context: Provide an official container image for toyssd so users can pull and run without building locally. Defer implementation for later; keep requirements and steps here for easy pickup.

Rationale:

- Faster onboarding: `docker run ghcr.io/cphurley82/toyssd:latest`
- Reproducible environments across macOS/Linux
- Enables CI/CD consumers or classroom/lab setups

Scope:

- Build multi-arch images (linux/amd64, linux/arm64) using GitHub Actions
- Push to ghcr.io/cphurley82/toyssd with sensible tags
- Update README with pull/run examples

Tasks:

1) Create CI workflow (e.g., `.github/workflows/publish-docker.yml`):
   - Triggers: on push of tags (v*), manual dispatch
   - Actions: `docker/setup-buildx-action`, `docker/login-action`, `docker/metadata-action`, `docker/build-push-action`
   - Platforms: `linux/amd64,linux/arm64`
   - Context: repo root (Dockerfile already present)
   - Caching: enable buildx cache (gha)
2) Auth: ensure `GHCR` publish permissions
   - Set `permissions: packages: write` in the workflow
   - No secrets needed for public repo when using GITHUB_TOKEN with GHCR
3) Tagging scheme:
   - On tags: `vX.Y.Z` → tags `X.Y.Z`, `X.Y`, `X`, `latest`
   - On main: moving tag `edge` (optional)
4) README updates:
   - Replace local build examples (or keep as Option B) with `docker run ghcr.io/cphurley82/toyssd:latest ...`
   - Document multi-arch availability
5) Optional hardening:
   - Cosign signing + provenance (SLSA) if desired
   - SBOM generation (syft) and attach as release asset

Acceptance criteria:

- Workflow publishes `ghcr.io/cphurley82/toyssd` for `linux/amd64` and `linux/arm64`
- Pulling and running the image executes unit tests successfully
- README shows working `docker run` examples using GHCR image

Suggested GitHub Issue (copy/paste):

Title: Publish multi-arch Docker image to GHCR via CI

Body:

- Goal: Provide an official container image (`ghcr.io/cphurley82/toyssd`) built on push of version tags and optionally on main.
- Requirements:
  - Multi-arch (amd64, arm64) with buildx
  - Tag mapping: vX.Y.Z → X.Y.Z, X.Y, X, latest; main → edge
  - Use GITHUB_TOKEN with `packages: write`
  - Update README to prefer GHCR pulls (retain local build as fallback)
- Nice-to-haves: cosign signing, SBOM
- Definition of Done: Image published; README updated; sample `docker run` tested.

Notes:

- Current Dockerfile already builds the project and a bundled fio; we’ll keep that for now.
- Consider a slimmer runtime image in future (two-stage build) to reduce image size.

---

## CI: Run short demo in pipeline

Goal: Exercise the end-to-end fio → engine → simulator path during CI with a very fast smoke, without slowing the pipeline.

Approaches:

- Option A (CTest switch): Enable a tiny demo test during configure and run it via ctest.

   - Configure: `-DTOYSSD_DEMO_TEST=ON`
   - Execute: `ctest -R FioDemoShort --output-on-failure`

- Option B (Explicit target): Invoke the demo target directly in the workflow.

   - Build then run: `cmake --build . --target run_fio_demo_short -j1`

Notes:

- Both options rely on the bundled fio if a system fio is not present. The CMake logic already builds it when needed.
- The short demo defaults to 1s runtime and 4M size; it’s designed to be non-flaky and quick.

Tasks:

1) Update CI workflow to add a new step after unit tests that runs either Option A or B in the container build context.
2) Ensure environment variables for dynamic libs are set by the target (already handled by the CMake target for macOS/Linux).
3) Keep runtime minimal and mark the step as non-blocking only if flakes are observed (otherwise keep it required).

Acceptance criteria:

- CI runs the short demo on every push/PR.
- The step finishes in under ~10–20 seconds on typical runners.
- Failures are surfaced in CI logs with fio output attached.

---

## Fix fio demo job and docs to use external engine correctly

Context: The fio job file `fio_plugin/demo_job.fio` specifies `ioengine=./libssdsim.so`, but for an external ioengine module it should be `ioengine=external:./libssdsim_engine.so`. The docs should reflect the `external:` form and environment variables.

Tasks:

1) Update `fio_plugin/demo_job.fio` to use the external engine module path:
   - `ioengine=external:./libssdsim_engine.so` (Linux) or `.dylib` on macOS if built as a module.
   - Keep `filename=config/default.json` as the simulator config path.
2) README corrections:
   - In manual examples, prefer `--ioengine=external:$BUILD_DIR/libssdsim_engine.so`.
   - Clarify that `SSD_SIM_LIB_PATH` should point to `libssdsim.{so,dylib}` and that `DYLD_LIBRARY_PATH/LD_LIBRARY_PATH` must include SystemC and the build dir (demo target already sets these).

Acceptance criteria:

- The job file runs with `fio --ioengine=external:... demo_job.fio` in the build directory.
- README examples match and successfully run on both Linux and macOS.

---

## Wire JSON config into simulator (geometry + timing)

Context: `config/default.json` and `tools/gen_config.py` exist, but the C API `ssdsim_init(config_path)` ignores the file. We should parse JSON and configure NAND geometry/timing and controller overhead.

Tasks:

1) Add a small JSON dependency (e.g., nlohmann/json via FetchContent) and a `Config` loader in C++.
2) Populate:
   - NAND: dies, blocks_per_die, pages_per_block, page_size_bytes, timing (t_read_us, t_prog_us, t_erase_us)
   - Controller: ctrl_overhead_us
3) Thread through to:
   - `Firmware::ctrl_overhead()` from loaded config
   - `NandModel` timing values (and optionally simple geometry checks)
4) Update `ssdsim_init` to parse the file and initialize modules accordingly; error if missing/invalid.
5) Document config schema in README and `docs/toy_ssd_simulator_design.md`.

Acceptance criteria:

- Passing different JSON values measurably changes latencies in the FioDemoShort run.
- Invalid config path or malformed JSON returns non-zero from `ssdsim_init` and logs an error.

---

## Correct latency units reported to fio

Context: `HostInterface::poll` currently assigns `ssd_cpl_t.ns` from `Completion.complete_ts.value()`, which is not a nanoseconds value and represents absolute simulation ticks, not per-request latency. fio expects completion latency or at least a consistent time base.

Tasks:

1) Track per-request start time and compute latency:
   - Option A: Add `sc_time start_ts` to `Completion` and fill in `Firmware` as `sc_time_stamp() - r->submit_ts` (preferred).
   - Option B: Keep start_ts in `IORequest` and compute latency when draining completions.
2) Convert `sc_time` to nanoseconds correctly (e.g., `uint64_t ns = (uint64_t)std::llround(latency.to_seconds() * 1e9);`).
3) Adjust `ssd_cpl_t.ns` to carry latency (ns). Document semantics in `ssdsim_api.h`.
4) Add a unit test asserting conversion correctness for at least one known delay (e.g., 50us read).

Acceptance criteria:

- fio stats align with configured timings (e.g., P50 ~50us for reads in default config).
- New unit test passes and protects against regressions in time conversion.

---

## Define thread-safety model and guard C API

Context: The C API may be called from multiple threads (fio jobs). Current implementation drives SystemC time advancement in `poll_cxx` with no synchronization and uses global state (`g_host`).

Tasks:

1) Clarify supported model (initially single-threaded fio job). Document in README and headers.
2) Add a lightweight mutex around `submit_cxx` and `poll_cxx` to serialize access to SystemC advancement and FIFOs.
3) Optionally add a runtime check guarding against multiple concurrent initializations.
4) Add stress unit/integration test with back-to-back submits and polls.

Acceptance criteria:

- No data races reported under TSAN when running tests (optional if available).
- Documented limitation or support statement for multi-job fio runs.

---

## Expand unit and integration test coverage

Context: Tests cover a simple FTL mapping case and minimal NAND timing. We need broader coverage to stabilize refactors.

Tasks:

1) FTL tests:
   - Wrap-around when `next_page` reaches `pages_per_block`.
   - Overwrite behavior: read mapping remains latest write.
   - Out-of-range/unknown LBA returns sentinel mapping.
2) NAND model tests:
   - Verify READ/PROGRAM/ERASE delays match config values.
   - Add basic data path check (optional in-memory buffer for a small space) to validate read-after-write.
3) Host/Firmware integration:
   - Submit N requests and ensure all complete with expected average latency.
4) CTest labels and `ctest -R` filters for fast vs. slow tests.

Acceptance criteria:

- New tests added under `tests/` and pass locally and in CI.
- `ctest -L fast` runs in < 5s locally.

---

## Logging and metrics pipeline

Context: `tools/analyze_results.py` and `tools/plot_metrics.py` are placeholders for CSV-based metrics, but the simulator doesn’t emit logs yet.

Tasks:

1) Add an optional CSV logger (compile-time option `TOYSSD_ENABLE_LOGGING`) that records per-IO: submit_ts, complete_ts, latency_us, lba, size, op.
2) Expose an environment variable or C API knob to enable logging and select output path.
3) Flesh out `tools/analyze_results.py` and `tools/plot_metrics.py` to consume the CSVs and produce simple summaries and plots.
4) Add a small section in README documenting how to enable and where logs appear.

Acceptance criteria:

- Running FioDemoShort produces a CSV when logging is enabled.
- Tools generate a summary and at least one histogram from the CSV.

---

## Build & CI improvements

Context: The project builds locally and in Docker. We should add first-class CI with caching and static analysis.

Tasks:

1) Add GitHub Actions workflow `.github/workflows/ci.yml`:
   - Matrix: ubuntu-latest (gcc, clang), macos-latest (AppleClang)
   - Steps: checkout, cache CMake/FetchContent, configure (Debug), build, run unit tests, optionally run `FioDemoShort`.
   - Use ccache for faster rebuilds.
2) Add static analysis jobs (non-blocking initially):
   - clang-tidy (with a minimal `.clang-tidy` config)
   - cppcheck (warnings only)
3) Add code coverage job (optional):
   - GCC + gcov + lcov to publish HTML artifact.
4) Cache FetchContent (`~/.cache/cmake`) and ccache.

Acceptance criteria:

- CI runs on PRs and main with status badges in README.
- Unit tests run under Linux and macOS; short demo runs on Linux.

---

## Coding standards and formatting

Context: No repository-wide formatting or linting configuration is present.

Tasks:

1) Add `.clang-format` (e.g., LLVM style with project tweaks) and format source tree.
2) Add `.clang-tidy` with a small ruleset (modernize-*, readability-*, bugprone-*), and a CMake target `clang-tidy`.
3) Add `pre-commit` config to run formatting and simple lint on changed files.
4) Document how to run format/lint locally and optionally add a CI check.

Acceptance criteria:

- Formatting is reproducible; CI fails on unformatted diffs if the check is enabled.
- clang-tidy target runs clean or with documented suppressions.

---

## CMake polish and packaging

Context: CMake is solid; a few quality-of-life improvements can help downstream users.

Tasks:

1) Add `CMAKE_EXPORT_COMPILE_COMMANDS` default ON for better IDE integration.
2) Set `SOVERSION` and `VERSION` properties on `ssdsim` for ABI tracking.
3) Consider `-fvisibility=hidden` with an export header for `ssdsim_api.h` (API macro), especially for Linux.
4) Add `toyssd::ssdsim` ALIAS target for consistency.
5) Add install of the fio engine module as an optional component (`TOYSSD_INSTALL_FIO_ENGINE`).

Acceptance criteria:

- `find_package(toyssd)` works in a separate project; exported targets resolve.
- Downstream can link to `toyssd::ssdsim` and include `ssdsim_api.h`.

---

## Dockerfile: multi-stage build and runtime image (follow-up)

Context: Current image is a dev container. For GHCR publishing we may want a slimmer runtime image.

Tasks:

1) Multi-stage: builder (full toolchain) + runtime (just libs + binaries + scripts).
2) Include runtime dependencies (SystemC libs, libstdc++, Python venv with tools if needed).
3) Provide an entrypoint to run unit tests or a demo by default; document overrides.

Acceptance criteria:

- Runtime image size is significantly smaller than builder.
- `docker run ghcr.io/...:latest` runs a smoke test or prints helpful usage.

---

## API hardening and documentation

Context: `ssdsim_api.h` is minimal; documenting semantics will reduce integration friction.

Tasks:

1) Add doxygen-style comments to the C API (ownership, alignment, thread-safety, error codes).
2) Define error codes enum and return behavior (e.g., EBUSY, EINVAL).
3) Provide a tiny C example using the API (in `api/examples/`), and wire into CI to build it.

Acceptance criteria:

- Generated docs (optional) or README snippet explains the API contract.
- Example builds and issues a couple of IOs successfully.

---

## Improve NAND interface layering (TLM)

Context: `NandInterfaceImpl` uses a non-standard shortcut (`dynamic_cast` to `NandModel`). We should move toward proper TLM GP or a clean abstract interface.

Tasks:

1) Define a TLM GP payload with extensions (die, block, page, op) and use `b_transport` uniformly.
2) Remove the direct `dynamic_cast` and rely on sockets/interfaces cleanly.
3) Keep a compile-time option to use the current shortcut while refactoring.

Acceptance criteria:

- NAND ops travel through a single TLM path with clear timing accounting.
- Unit tests updated accordingly.

---

## Mac-specific notes and environment handling

Context: Demos set `DYLD_LIBRARY_PATH` and load `scmain_stub` on non-Darwin platforms via `LD_PRELOAD`. Let’s ensure the behavior is robust.

Tasks:

1) In CMake demo targets, verify and document environment variables set for both platforms.
2) Add a brief troubleshooting section to README for common macOS dynamic loader issues.

Acceptance criteria:

- Users on macOS can run `run_fio_demo` without manual env exports.
- Troubleshooting doc reduces recurring issues.

---

## Developer ergonomics

Context: Make day-to-day development smoother.

Tasks:

1) Add a `make dev` style target or `justfile` for common flows (configure, build, test, demo).
2) Add VS Code settings/tasks for CTest and demo targets; ensure `compile_commands.json` is emitted.
3) Add a `CONTRIBUTING.md` with coding style and PR checklist.

Acceptance criteria:

- New contributors can build and run quickly with a short set of commands.
- Editor integration (intellisense) works out-of-the-box.

---

## README: Add useful status badges

Context: README currently shows the formatting workflow badge. Add other common badges to improve project discoverability and status visibility.

Candidate badges:

- Build/CI status (full build + tests) for `main`
- Unit tests badge (if split workflow is desired)
- Code coverage (Codecov or GitHub Actions summary)
- Latest release / version tag
- License (MIT)
- Docker image (GHCR pulls / image size) after publishing
- clang-tidy or static analysis status (if we add a job)

Tasks:

1) Decide on desired badge set and placement (top of README under title).
2) Add/adjust workflows as needed to support badges:
   - CI build/tests (if not already present)
   - Coverage uploader (Codecov) and configure repository (tokenless for public)
   - Docker publish (when implemented) to enable an image badge
3) Update `README.md` to include markdown for each badge with correct links.
   - Examples: GitHub Actions workflow badges, Codecov, License, GHCR image
4) Verify badges render on GitHub and link to the correct pages (workflows, coverage, package page).

Acceptance criteria:

- README shows the selected badges immediately under the project title.
- Clicking each badge navigates to the correct workflow run, coverage report, releases page, license, or GHCR package.
- Badges stay green on passing CI and reflect current repository state.
