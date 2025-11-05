# Contributing to toyssd

Thanks for your interest in helping build the `toyssd` simulator! This guide captures the workflow, coding standards, and review expectations for contributors.

## Code of Conduct

Be respectful, collaborative, and empathetic. We want a welcoming environment for everyone experimenting with SSD simulation.

## Getting Started

1. **Fork & clone** the repository.
2. **Bootstrap tooling** with `uv run invoke bootstrap`.
3. **Create a branch** for your work: `git checkout -b feature/my-change`.

## Development Workflow

### Build & Test Frequently

```bash
uv run invoke build    # Configure & compile C++ components
uv run invoke test     # Run GoogleTests and Python tests
```

Pass both commands before opening a pull request. To accelerate iterations, build in `Release` or incremental mode via `uv run invoke build --config Release`.

### Lint & Format

We keep the tree clean and consistent. Run before submitting:

```bash
uv run invoke format
uv run invoke lint
```

These commands cover clang-format, clang-tidy, cpplint, and Ruff. If you intentionally need to suppress a warning, document the rationale in code comments.

### Python Environment Notes

- uv manages the `.venv` automatically; avoid committing it.
- Prefer Python 3.11 for now. uv pins compatible wheels for SystemC/PySysC integration.
- Install the Xcode Command Line Tools (`xcode-select --install`). Invoke tasks set `CC=/usr/bin/clang` and `CXX=/usr/bin/clang++` so the Apple-provided SDK-aware toolchain is used when compiling SystemC.

## Clean Code Principles

We strive to follow clean code principles across both C++ and Python:

- Prefer clear, intention-revealing names and consistent terminology.
- Keep functions/classes small with a single, focused responsibility.
- Avoid duplication (DRY) and remove dead or obsolete code.
- Prefer composition over inheritance where it improves clarity.
- Minimize mutable shared state; make side effects and data flow explicit.
- Fail fast with meaningful errors; handle edge cases deliberately.
- Write tests alongside code and refactor confidently.

These practices complement the language-specific guidance below and are reinforced by our format and lint tooling.

### C++ Guidelines

- Follow the Google C++ Style Guide (mirrors clang-format/clang-tidy defaults in the repo).
- Use modern C++20 features where appropriate (e.g., `std::span`, `std::optional`, `constexpr`).
- Keep translation units small and headers self-contained.
- Add focused GoogleTests for new logic (host/controller/nand behavior, TLM extensions).

### Python Guidelines

- Follow the Google Python Style Guide + Ruff linting rules.
- Favor dataclasses and explicit type hints for public APIs.
- Add pytest coverage for new Python modules and the `toyssd` API surface.

## Documentation

- Update `docs/ssd_sim_design.md` when design decisions change.
- Extend `README.md` for large features that affect onboarding.
- Include docstrings for new Python APIs and inline comments for non-obvious C++ logic.

## Pull Request Checklist

- [ ] Tests pass locally (`uv run invoke test`).
- [ ] Lint/formatting is clean (`uv run invoke lint` / `format`).
- [ ] New or updated functionality covered by tests.
- [ ] Documentation updated where needed.
- [ ] Commit messages are clear and reference relevant issues.

## Review Process

1. Open a PR describing motivation, approach, and testing.
2. Expect actionable feedback focused on correctness, maintainability, and consistency with the design doc.
3. Address review comments promptly; prefer amending your branch rather than force-pushing unrelated changes.

## Release Notes

We maintain a running changelog in the design doc until a formal `CHANGELOG.md` is added. Call out user-facing changes in your PR description.

## Questions?

Open a GitHub issue or start a discussion thread. If you are unsure about an approach, share early sketches in the issue tracker before writing code.

Happy coding!
