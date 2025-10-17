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
