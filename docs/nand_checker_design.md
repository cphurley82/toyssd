# NAND Checker Framework Design

Version: 0.1 (design only)

Audience: simulator developers, firmware/verification engineers

Status: design approved for implementation; no production code yet

---

## Overview (Why checkers?)

To ensure the SoC firmware and hardware exercise the NAND model correctly and to catch protocol, geometry, and timing misuse early, we introduce a modular checker framework that validates operations at runtime, independent of the NAND functional model. The framework is configurable (JSON, programmatic, CLI) and testable in isolation.

---

## Goals and non-goals

### Goals

- Validate NAND usage rules: geometry limits, protocol ordering, timing windows, bus sharing, and resource concurrency.
- Be modular and composable; each rule is a self-contained checker class.
- Be configurable from JSON and programmatically, with per-checker severity and thresholds.
- Provide rich diagnostics (who/what/when/where/why) and actionable guidance.
- Be unit-testable without running SystemC.
- Add near-zero risk of changing NAND behavior unless configured to “block on error”.

### Non-goals

- Full physical NAND accuracy or ECC correctness modeling.
- Replacing the timing model; checkers observe and validate rather than simulate timing.
- Implementing host/FTL policy checks; those belong to separate layers.

---

## High-level architecture

```text
[Firmware] -> NandCmd -> [NandModel]
                         |        
                         v        
                    [CheckerManager]
                      |    |    |      
                 [Checker]...[Checker]
```

- CheckerManager routes events from the NAND model to a set of IChecker instances.
- Each checker is stateless or stateful as needed (e.g., tracking per-die busy state).
- Checkers emit violations with severity and context; a policy determines reporting and whether to block the operation.

---

## Event model and lifecycle

The framework observes a small set of canonical events:

- CommandEnqueue(op, addr, sizes, now): Intent to issue an operation (pre-check).
- CommandStart(op, addr, now): The operation starts on the device/bus.
- CommandComplete(op, addr, started_at, done_at): Operation finished.
- ChannelArb(phase, channel, ce, now): Channel bus acquisition/release (optional, derived by the NAND model if available).
- Tick/TimeAdvance(now): Optional beat for timing-window checks if not driven by start/complete.

At a minimum, NandModel should call: pre-check (CommandEnqueue) before carrying out the command; post-check (CommandComplete) when done. If the model has an internal notion of start time, it should also call CommandStart.

---

## Interfaces (C++ sketch; not implemented)

```cpp
// Severity and policy for violations
enum class CheckSeverity { Info, Warn, Error, Fatal };

struct Violation {
  std::string checker;    // e.g., "GeometryChecker"
  CheckSeverity severity; // effective after overrides
  std::string code;       // short stable code, e.g., "ADDR.OOB"
  std::string message;    // human-readable detail
  NandCmd cmd;            // snapshot for context
  sc_core::sc_time when;  // event time
};

struct CheckerContext {
  // Geometry, sizes, timing knobs, device topology, etc.
  // Read-only during checks.
  // Includes a reference to a thread-safe reporter/logger.
};

struct IChecker {
  virtual ~IChecker() = default;
  virtual const char* name() const = 0;

  // Return optional Violation (single) or accumulate via reporter in ctx
  virtual std::optional<Violation> on_enqueue(const NandCmd&, const CheckerContext&, sc_core::sc_time now) { return std::nullopt; }
  virtual std::optional<Violation> on_start(const NandCmd&, const CheckerContext&, sc_core::sc_time now) { return std::nullopt; }
  virtual std::optional<Violation> on_complete(const NandCmd&, const CheckerContext&, sc_core::sc_time started, sc_core::sc_time now) { return std::nullopt; }
};

class CheckerManager {
 public:
  void configure(const CheckerConfig& cfg);
  void add(std::unique_ptr<IChecker> chk);

  // Returns true if operation should proceed; false if blocked by policy
  bool pre_check(const NandCmd& cmd, sc_core::sc_time now);
  void on_start(const NandCmd& cmd, sc_core::sc_time now);
  void on_complete(const NandCmd& cmd, sc_core::sc_time started, sc_core::sc_time now);

  // Reporting, counters, snapshots
  const Metrics& metrics() const;
};
```

Notes

- All interfaces are header-only friendly; checkers can live in separate compilation units.
- For SystemC-free unit tests, substitute `sc_core::sc_time` with a lightweight shim or use zero time.

---

## Policy and reporting

- Each checker emits violations with a default severity.
- The manager applies overrides from configuration:
  - enable/disable checker
  - per-code severity remap (e.g., treat `ADDR.OOB` as Fatal)
  - action: log, count, throw (block), or `sc_report` (configurable).
- Reporting sinks
  - sc_report (INFO/WARNING/ERROR)
  - stderr/stdout
  - JSON Lines file for machine consumption
  - Metrics counters per checker and per code

---

## Checker catalog

Below is the initial set of concrete checkers; each lists purpose, triggers, config, and typical violation codes.

### GeometryChecker

- Purpose: Verify address fields fall within configured geometry.
- Triggers: on_enqueue
- Rules:
  - 0 ≤ channel < num_channels
  - 0 ≤ ce < ce_per_channel
  - 0 ≤ lun < luns_per_ce
  - 0 ≤ plane < planes_per_lun
  - 0 ≤ block < blocks_per_plane
  - 0 ≤ wordline < wordlines_per_block
  - 0 ≤ logical_page < logical_pages_per_wordline
- Config:
  - geometry dimensions (required)
  - allow_reserved_blocks: [ranges]
  - severity per field (e.g., wordline overflow = Fatal)
- Violations: ADDR.OOB.{CH,CE,LUN,PLN,BLK,WDL,LP}

### DataLengthChecker

- Purpose: Verify data and metadata buffers match configured sizes.
- Triggers: on_enqueue
- Rules:
  - If data provided: data.size == page_data_bytes
  - If metadata provided: metadata.size == oob_bytes
  - Optional: allow partial transfers (disabled by default)
- Config: allow_partial_data, allow_partial_oob
- Violations: IO.SIZE.DATA, IO.SIZE.OOB

### ProtocolOrderChecker

- Purpose: Enforce program-after-erase and forbid program-over-program.
- Triggers: on_enqueue (fast fail), optionally on_complete (state update)
- Rules per physical page:
  - State ∈ {Erased, Programmed, Invalid}
  - PROGRAM requires Erased; READ allowed when Programmed or Erased (config)
  - ERASE transitions all pages in block to Erased
- Config: read_from_erased_ok (Warn or Error)
- Violations: PROTO.PGM.NO_ERASE, PROTO.PGM.REPROG, PROTO.RD.UNINIT

### ProgramSequenceChecker

- Purpose: For MLC/TLC, enforce lower→upper or step order within a wordline.
- Triggers: on_enqueue
- Rules: logical_page must follow configured ordering and maximum times
- Config: steps_per_wordline (e.g., 1=SLC, 2=MLC, 3=TLC), sequence table
- Violations: PROTO.PGM.SEQ, PROTO.PGM.OVERSTEP

### EraseAtomicityChecker

- Purpose: Ensure ERASE is block-scoped; ignore page fields and unify behavior.
- Triggers: on_enqueue/on_complete
- Rules: ERASE commands referencing any wordline in block apply to entire block
- Config: require_wordline_zero_for_erase (warn if not zero)
- Violations: ERASE.ADDR.MISMATCH

### BadBlockChecker

- Purpose: Disallow use of known-bad blocks.
- Triggers: on_enqueue
- Rules: Block ∉ bad_block_table for this plane
- Config: bad_block_table (list/ranges), severity
- Violations: ADDR.BAD_BLOCK

### ChannelBusChecker

- Purpose: Validate one active command per channel bus and report utilization.
- Triggers: on_start/on_complete (needs start/complete timing), optional bus acquire/release
- Rules: Prevent overlapping on same channel; report dwell time per command
- Config: check_overlap=true, report_utilization=true
- Violations: BUS.CHANNEL.OVERLAP

### DieBusyChecker

- Purpose: Ensure per-die (CE/LUN) concurrency obeys device limits.
- Triggers: on_start/on_complete
- Rules: For a given (channel,ce,lun), no overlapping PROGRAM/READ/ERASE
- Config: allow_read_during_prog=false (advanced devices may permit)
- Violations: BUS.DIE.OVERLAP

### PlaneRulesChecker

- Purpose: Validate multi-plane constraints (if/when introduced).
- Triggers: on_enqueue
- Rules: Addresses in multi-plane ops must share block/wordline alignment
- Config: enable=false by default; requires multi-plane command family
- Violations: PLANE.ADDR.MISALIGN

### TimingWindowChecker

- Purpose: Basic min-gap checks between sequential ops on same resource.
- Triggers: on_complete (uses start/done times); optionally on_start
- Rules (examples):
  - tWB: min write-to-busy before PROGRAM takes effect
  - tBERS, tPROG, tR: enforce minimum busy durations if model doesn’t already
  - tADL: address to data loading min interval
- Config: per-parameter thresholds in sc_time
- Violations: TIME.MIN.{WB,BERS,PROG,R,ADL}

### OobConsistencyChecker

- Purpose: Basic metadata invariants (e.g., ECC header size, reserved bytes zero)
- Triggers: on_enqueue for PROGRAM; on_complete for READ validation
- Config: layout map for OOB fields; per-field policy (Any/Zero/NonZero)
- Violations: OOB.LAYOUT.{FIELD}

### WearoutCounterChecker

- Purpose: Track P/E cycles per block and warn on thresholds
- Triggers: on_complete(ERASE)
- Config: warn_at, error_at (cycles)
- Violations: WEAR.PE.THRESH

---

## Configuration

Configuration is layered with the following precedence (lowest to highest):

1. Built-in defaults
2. JSON config file (e.g., `config/default.json`)
3. Environment/CLI overrides
4. Programmatic overrides via API

### JSON schema (excerpt)

```json
{
  "nand": {
    "geometry": {
      "channels": 2,
      "ce_per_channel": 2,
      "luns_per_ce": 1,
      "planes_per_lun": 2,
      "blocks_per_plane": 1024,
      "wordlines_per_block": 256,
      "logical_pages_per_wordline": 3,
      "page_data_bytes": 16384,
      "oob_bytes": 1024
    },
    "checkers": {
      "defaults": { "severity": "Warn", "enabled": true },
      "GeometryChecker": { "severity": "Fatal" },
      "DataLengthChecker": { "enabled": true },
      "ProtocolOrderChecker": { "read_from_erased_ok": "Warn" },
      "ProgramSequenceChecker": { "steps_per_wordline": 3 },
      "BadBlockChecker": { "bad_blocks": [[7, 13], 42] },
      "TimingWindowChecker": {
        "tWB_ns": 100,
        "tR_ns": 30000,
        "tPROG_us": 800,
        "tBERS_ms": 3
      }
    }
  }
}
```

### Programmatic configuration (sketch)

```cpp
CheckerConfig cfg;
cfg.geometry = {/* ... */};
cfg.enable("GeometryChecker").severity(CheckSeverity::Fatal);
cfg.get<ProtocolOrderChecker>().read_from_erased = CheckSeverity::Warn;
checker_manager.configure(cfg);
```

### CLI/environment (suggested)

- --checkers.enable=GeometryChecker,ProtocolOrderChecker
- --checkers.GeometryChecker.severity=Fatal
- --checkers.ProtocolOrderChecker.read_from_erased=Warn
- SSD_CHECKERS=+GeometryChecker,-TimingWindowChecker

---

## Integration with NandModel

Minimal, non-invasive touch points in `NandModel::b_transport`:

- Before executing a command:
  - Build `NandCmd` (already done by caller) and call `checker_mgr.pre_check(cmd, now)`.
  - If it returns false (policy blocks), set an error on the payload or raise an `sc_report` and return.
- When operation begins on the device (if modeled): `checker_mgr.on_start(cmd, now)`.
- When operation completes: `checker_mgr.on_complete(cmd, started, now)`.

Optional: surface `CheckerManager` as a submodule or injectable dependency when constructing `NandModel` so that unit tests can provide a fake manager.

---

## Testing strategy

- Unit tests per checker (GoogleTest):
  - Happy path and boundary conditions (e.g., channel==max-1 is OK; channel==max is OOB).
  - Property tests for ranges and sequences (e.g., program order permutations).
- Integration tests with the real model:
  - Use tiny geometry and deterministic timings.
  - Inject known-bad sequences to assert violations are raised.
- Determinism:
  - All time inputs come from test harness; no wall-clock dependencies.
- Coverage:
  - Aim for >90% per checker; keep state small and explicit.

---

## Metrics and reporting

- Per-checker counters: total, by severity, by code.
- Resource utilization:
  - ChannelBusChecker reports bus occupancy time and max concurrency.
  - DieBusyChecker reports average queue length (if tracked) or overlap attempts.
- Export formats:
  - Human logs via sc_report
  - Machine logs via JSONL: one line per violation
  - Optional CSV summaries at end of run

---

## Performance considerations

- O(1) checks per event; use flat arrays keyed by compact resource IDs (channel, ce, lun, plane) to avoid hash overhead in hot paths.
- Make all checkers optional; disable all for performance-critical runs.
- Avoid heap allocations in steady state; pre-size state using geometry.

---

## Example end-to-end flow (sketch)

```cpp
NandModel nand{"nand"};
auto mgr = std::make_shared<CheckerManager>();
mgr->configure(load_checker_config_from_json("config/default.json"));
nand.set_checker_manager(mgr); // new setter on NandModel

NandCmd cmd{ .op = NandCmd::Op::PROGRAM, .addr = {/*...*/}, .data = data, .metadata = oob };
sc_core::sc_time now = sc_core::SC_ZERO_TIME;
// inside b_transport:
if (!mgr->pre_check(cmd, now)) return; // blocked by policy
mgr->on_start(cmd, now);
// ... execute timing model ...
mgr->on_complete(cmd, started, now + tPROG);
```

---

## Future extensions

- Multi-plane, die-interleave, and copyback command families with corresponding checkers.
- Formalized property sets (e.g., SVA-like) expressed in a DSL that generates checker code.
- Trace correlation to host requests for cross-layer assertions.

---

## Appendix: Canonical violation codes

- ADDR.OOB.{CH,CE,LUN,PLN,BLK,WDL,LP}
- ADDR.BAD_BLOCK
- IO.SIZE.{DATA,OOB}
- PROTO.PGM.{NO_ERASE,REPROG,SEQ,OVERSTEP}
- PROTO.RD.UNINIT
- ERASE.ADDR.MISMATCH
- BUS.CHANNEL.OVERLAP
- BUS.DIE.OVERLAP
- PLANE.ADDR.MISALIGN
- TIME.MIN.{WB,BERS,PROG,R,ADL}
- OOB.LAYOUT.{FIELD}
- WEAR.PE.THRESH

This document defines a modular, configurable checker framework that observes NAND operations and enforces correctness constraints while producing actionable diagnostics. It is intentionally implementation-agnostic but aligned with the current `NandModel` API and SystemC/TLM usage in this repository.
