# Agent rules for this repo

These lightweight rules help keep changes safe and green when using AI coding assistants (e.g., GitHub Copilot Chat) in VS Code.

Guiding principles

- Keep changes small and cohesive; prefer focused patches that compile and pass tests.
- Prefer editing existing code over large rewrites.
- Update nearby docs/tests when behavior changes.

Required checks after each substantive change

1) Run the unified verification flow locally:

   ```bash
   uv run invoke verify
   ```

   This performs static checks (C++/Python), configures/builds C++, and runs tests via CTest/pytest.

2) If verification fails, iterate until green. Don’t leave the repo in a failing state.

## When touching C++

- Build Debug first (default); prefer `build-debug` as the local build dir.
- If using VS Code, you can run the workspace task “Verify” or “Build + Verify (Debug)”.

## When touching Python

- Run `uv run invoke py_format` and `uv run invoke py_lint`.

Automation in this repo

- VS Code tasks: `Verify` runs the full flow; `Build + Verify (Debug)` also configures/builds Debug first. See `.vscode/tasks.json`.

Notes for agents

- After editing files, prefer running the `Verify` task instead of ad‑hoc commands, so output is captured consistently in the terminal.
- If verification introduces many warnings/errors, fix the minimal root cause first; avoid cascading changes.
