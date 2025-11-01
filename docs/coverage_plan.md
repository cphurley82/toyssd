# Test Coverage Plan and Dead Code Review

This document tracks how we will raise unit test coverage to ≥100% for core code, and records findings from a quick dead/unused code sweep.

Baseline (as verified recently):

- macOS (AppleClang, gcovr): ~27–28% lines
- Docker/CI (Ubuntu 24.04, gcovr): ~30–31% lines
- CI/Local threshold: 20% (configurable)

Coverage tooling:

- Enable with CMake: `-DENABLE_CODE_COVERAGE=ON -DCODE_COVERAGE_THRESHOLD=<N>`
- Run: `cmake --build <build-dir> --target coverage`
- Reports: `<build-dir>/coverage/index.html` (HTML) and `coverage.xml` (XML)

## TODO (next steps)

- [ ] NAND: add boundary and error-path tests (first/last block/page, OOB indices)
- [ ] NAND: verify state transitions (program → read → erase; double-program behavior)
- [ ] FTL: add mapping lifecycle tests (write→readback, overwrite remap)
- [ ] FTL: space pressure and failure/GC behavior; reads of never-written LBAs
- [ ] HostInterface: dispatch happy-path + invalid-request tests
- [ ] C API: init/teardown, minimal write+read loop, invalid config paths
- [ ] Decide on `sim/util/Logger.h` (remove vs. adopt and use under NDEBUG guards)
- [ ] README: clarify coverage example (default threshold currently 20%)
- [ ] CI: consider enabling `-Wunused-*` and `-Wunreachable-code`; add clang-tidy pass
- [ ] Plan to ratchet coverage threshold to 40% once the above land and pass in CI
- [ ] Split oversized tests if needed (e.g., create `tests/test_ftl_advanced.cpp`)
- [ ] Add targeted death tests for hard contracts (preconditions/invariants)

## Scope and principles

- Focus on deterministic, fast GTest tests that don’t require the fio engine.
- Exercise public behavior through small, composable units; avoid brittle internals.
- Prefer parameterized tests, edge cases, and negative/error paths.
- Use death tests for contract violations and `EXPECT_THROW` for recoverable errors.
- Keep SystemC time control explicit (stepping or mocking interactions) to ensure determinism.

## Module-by-module plan

The lists below are ordered to maximize coverage gain quickly. Each item suggests the most valuable scenarios and edge cases.

### NAND subsystem (sim/nand)

Targets: `NandModel`, `NandInterface`, `NandInterfaceImpl`

High-value tests:

- Addressing and bounds
  - Valid page program/read flows across first/last blocks/pages
  - Out-of-range block/page/chip indices → proper error handling or guards
- State transitions
  - Program → read → erase → read-empty behavior
  - Program to same page twice (should fail or overwrite? verify current behavior)
- Metadata and wear
  - Erase count increments and exposure (if available)
  - Read of erased page returns expected empty pattern
- Error propagation
  - Interface-level invalid ops propagate errors from model consistently

Tooling:

- Use `nand_tests` (already present) as the home for these; split cases into small `TEST()` blocks and parameterize over boundary values.

### FTL + Firmware (sim/fw)

Targets: `FTL`, `Firmware`

High-value tests:

- Mapping lifecycle
  - First write to LBA maps to a fresh physical page; subsequent read returns the same data
  - Overwrite same LBA → new mapping and old page invalidation (if modeled)
- Space pressure
  - Sequential writes until full; verify allocation and failure mode or trigger GC if present
- TRIM/Discard (if supported)
  - TRIM invalidates mapping; subsequent read yields not-found/empty
- Error cases
  - Reads of never-written LBAs
  - Writes with invalid sizes/alignments
  - Propagation of lower-level (NAND) failures

Tooling:

- Use `unit_tests` binary; add a new `test_ftl_advanced.cpp` if the existing `test_ftl.cpp` grows too large.

### Host interface (sim/host)

Target: `HostInterface`

High-value tests:

- Command dispatch
  - Well-formed read/write requests reach the firmware/FTL with correct parameters
  - Invalid opcodes/parameters rejected with clear status
- Concurrency model (if any)
  - Ensure reentrancy guards or sequencing expectations are upheld

Implementation hints:

- Introduce a tiny mock/stub for FTL/Firmware within tests to assert interactions without SystemC scheduling overhead.

### C API (api/)

Target: `ssdsim_api` (exposed to fio engine)

High-value tests:

- Init/teardown
  - Initialize with a valid JSON config and teardown cleanly (no leaks/crashes)
- Basic I/O lifecycle
  - Submit a minimal write + read through the API and verify data loopback
- Config variants
  - Invalid/missing config paths → clear errors
  - Edge parameters (e.g., minimal geometry) if accepted by schema

Tooling:

- Link tests to `ssdsim` if calling the C API directly, or keep through `simlib` if API is a thin wrapper; prefer the thinnest integration that remains deterministic.

## Exclusions and special cases

- External deps (`_deps/`), generated build trees (`build*/`), and test sources are excluded from coverage by default.
- `sc_main_stub.cpp` intentionally appears in two targets:
  - As part of `ssdsim` to satisfy dynamic loading in contexts expecting an `sc_main` symbol.
  - As a separate `scmain_stub` library for Linux `LD_PRELOAD`-style use with fio.
  This duplication is intentional and not a dead-code smell.

## Dead/unused code review (quick sweep)

Methodology:
 
- Compared source files used in CMake targets against the tree.
- Grepped for common unused hints: `[[maybe_unused]]`, `UNUSED`, `deprecated`, and `TODO remove`.
- Looked for headers that aren’t included anywhere.

Findings:
 
- sim/util/Logger.h
  - Observation: Not included anywhere; symbol `log_info(...)` is not referenced.
  - Evidence: `grep -R "#include .*Logger.h"` and `grep -R "\blog_info\("` found no hits outside the header itself.
  - Recommendation:
    - Option A: Remove `sim/util/Logger.h` from the repo until we need it.
    - Option B: Adopt it as a minimal logging facility and start using it in low-level modules (e.g., debug paths guarded by `#ifndef NDEBUG`).
- Headers listed in targets
  - `sim/Top.h` is listed among `simlib` sources but is also included by `sim/main.cpp` and `sim/host/HostInterface.cpp`; this is benign. Keeping headers in target sources helps format/lint tooling; no action required.
- No other obvious unused markers
  - No explicit `[[maybe_unused]]`/`UNUSED` or deprecation markers found by a quick grep.

Next-level dead code checks (optional follow-ups):
 
- Enable `-Wunused-function -Wunused-parameter -Wunreachable-code` and review warnings in CI.
- Run `clang-tidy` with checks like `misc-unused-parameters`, `cppcoreguidelines-avoid-magic-numbers` (style), and enable readability maintainers prefer.
- Consider a lightweight include-what-you-use (IWYU) pass to trim accidental includes.

## Threshold ratcheting strategy

- Keep threshold at 20% while adding tests in small PRs.
- When coverage stabilizes >50%, raise to 40–50%.
- Repeat until 80–90%.
- For the final push to 100% of in-scope code, decide explicitly on exclusions (e.g., rarely executed error branches or truly platform-specific glue) and document them here before raising to 100%.

## Milestones and tracking

- M1: NAND boundaries + error paths (expect +10–15%)
- M2: FTL mapping + overwrites + readback (expect +15–25%)
- M3: API init/teardown and basic I/O loop (expect +10%)
- M4: Host interface dispatch + negative tests (expect +5–10%)
- M5: Tail cleanup, death tests, and exclusions finalized → raise threshold progressively to 100%

Ownership:
 
- Prefer small, focused PRs (5–10 tests each). Keep HTML report links in PR descriptions for quick review.

## How to run

```bash
# macOS/Linux (example)
cmake -S . -B build-coverage -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_CODE_COVERAGE=ON -DCODE_COVERAGE_THRESHOLD=20
cmake --build build-coverage -j
cmake --build build-coverage --target coverage
open build-coverage/coverage/index.html   # macOS
# xdg-open build-coverage/coverage/index.html  # Linux
```

## Notes

- The README currently shows an example with `-DCODE_COVERAGE_THRESHOLD=60`; the project default is 20. Either update the snippet to 20 or explicitly call out that it’s an example value.
- If AppleClang is used, gcovr will prefer `llvm-cov gcov` when `llvm` is installed via Homebrew.
