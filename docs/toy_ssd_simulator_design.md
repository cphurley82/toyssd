---
title: Toy SSD Simulator Design Document
version: 0.1
authors: Chris Hurley
date: 2025-10-16
---

## SSD Simulator — Design Document

## Table of Contents

1. [Overview](#overview)
2. [Long-Term Goals](#long-term-goals)
3. [Development Philosophy](#development-philosophy)
4. [System Architecture](#system-architecture)
5. [Initial Design & Phased Plan (Outside-In)](#initial-design--phased-plan-outside-in)
6. [Detailed Architectural Specification](#detailed-architectural-specification)
7. [Test-Driven Development (TDD)](#test-driven-development-tdd)
8. [Tooling & Supporting Scripts](#tooling--supporting-scripts)
9. [Migration Path to Zephyr & RISC-V](#migration-path-to-zephyr--risc-v)
10. [Risks & Mitigation](#risks--mitigation)
11. [Next Steps](#next-steps)
12. [Appendix: Key Dependencies](#appendix-key-dependencies)

---

## Current Status

- Build system stabilized with CMake (C++20), SystemC/TLM, and GoogleTest.
- Docker image provides reproducible builds and test runs; CI (GitHub Actions) runs unit tests plus two demos inside Docker: a short ctest-based smoke (`FioDemoShort`, enabled via `TOYSSD_DEMO_TEST=ON`) and the longer parameterized CMake target (`run_fio_demo`).
- fio external ioengine (`ssdsim_engine`) is wired to `libssdsim` and produces completions against a stubbed SystemC pipeline.
- A CMake demo target is available: `run_fio_demo` (also aliased as `demo`) which runs fio with the external engine and a sample config.
- Progress through Phases 0–1: ioengine stub + HostInterface/Firmware scaffold with fixed-latency completions. NAND/FTL work is upcoming.


---

## Overview

This document defines the architecture and phased plan for developing an **SSD Simulator** that models controller firmware, host interface, and NAND flash behavior using **SystemC/TLM 2.0**.
The simulator will integrate with **fio** to replay real-world I/O workloads and evolve into a complete SSD platform supporting **RISC-V firmware execution under Zephyr**.

Core technologies include:

- **SystemC/TLM 2.0** for timing-accurate modeling
- **C++20** for firmware and simulation code
- **GoogleTest** for unit/integration testing
- **Python** for configuration, automation, and analysis
- **fio** as the host workload generator

---

## Long-Term Goals

### Functional Goals

- Simulate end-to-end SSD operation (host → controller → NAND).
- Run real **fio** workloads through a **custom ioengine**.
- Integrate a **RISC-V core (DBT-RISE)** running **Zephyr** firmware.
- Model realistic NAND timing and operations (read/program/erase).
- Provide reproducible, deterministic results for research and development.

### Engineering & Research Goals

- Modular and extensible design for FTL, garbage collection, and wear-leveling research.
- Clear firmware/hardware boundaries.
- Pluggable NAND models and interface layers.
- Performance metrics and traceability for all operations.
- Seamless integration with Python-based tools for data analysis and visualization.

---

## Development Philosophy

### Outside-In Approach

Development progresses from the **external host interface** inward:

1. **fio integration** through custom ioengine.
2. **Host interface** SystemC module for I/O dispatch.
3. **Firmware stub (C++)** simulating controller logic.
4. **NAND interface (TLM)** for firmware-to-hardware communication.
5. **NAND model** simulating physical NAND behavior.

Each phase ensures an operational simulator, enabling continuous end-to-end testing.

### Modern C++ Standards

- C++20 features: smart pointers, `constexpr`, ranges, structured bindings.
- RAII-based resource management and no manual memory handling.
- Use of STL containers and strong typing (e.g., `enum class` for NAND ops).
- Consistent naming, `clang-format` compliance, and `clang-tidy` checks.

### Core Principles

- **Determinism** — Simulation results reproducible by controlling timing and seeds.
- **Testability** — All modules testable in isolation via GoogleTest.
- **Traceability** — Logging of events and metrics for every I/O operation.
- **Modularity** — Firmware, host, and NAND independently replaceable.

---

## System Architecture

```text
[fio] –ioengine–> [libssdsim.so]
|
v
[HostInterface (SystemC module)]
|
v
[Firmware (C++ SC_MODULE)]
|  (contains FTL)
|  calls NAND interface API
v
[NandInterfaceImpl (TLM initiator socket)]
|
v
[NandModel (TLM target, timing model)]
```

### Technology Stack

| Component | Technology |
|------------|-------------|
| Simulation | SystemC 2.3+ (TLM 2.0) |
| Language | C++20 |
| Testing | GoogleTest |
| External tools | Python 3 |
| Host workload | fio (custom ioengine) |
| Build system | CMake |
| Static analysis | clang-tidy, cppcheck |

---

## Initial Design & Phased Plan (Outside-In)

| Phase | Description | Deliverable |
|-------|--------------|-------------|
| **0** | fio ioengine stub | Dummy C API returns instant completions |
| **1** | HostInterface + Firmware stub | Basic SystemC harness with fixed latency |
| **2** | TLM NAND Interface + NAND Model | Read/Program/Erase timing model |
| **3** | FTL inside Firmware | Page-mapped L2P table with linear allocation |
| **4** | Timing realism + metrics | Per-die concurrency, logging, and CSV traces |
| **5** | RISC-V + Zephyr integration | Replace native firmware with DBT-RISE core |

---

## Detailed Architectural Specification

### Core Modules Overview

#### 1. HostInterface

- C ABI for fio plugin communication (`libssdsim.so`).
- Converts C requests into `IORequest` objects and queues them in SystemC FIFOs.
- Provides non-blocking polling for completions.

#### 2. Firmware

- SC_MODULE representing controller logic and firmware execution.
- Contains:
  - **FTL** for LBA→PPA mapping.
  - Queueing and scheduling.
  - Calls **NAND Interface** for I/O execution.
- Emits completions to HostInterface.

#### 3. FTL

- **Stage 1:** page-mapped, sequential allocator.
- **Stage 2:** block reuse, garbage collection.
- **Stage 3:** hybrid mapping and wear-leveling.

#### 4. NAND Interface

- Abstract C++ API used by firmware.
- TLM bridge to communicate with the NAND model.

#### 5. NAND Model

- TLM target with configurable timing and geometry.
- Optional in-memory data array for end-to-end read/write validation.
- Latency parameters (`t_read`, `t_prog`, `t_erase`) configurable via JSON.

---

## Test-Driven Development (TDD)

### Testing Framework

- **GoogleTest** used for unit and integration testing.
- **CTest** integrated for automation.
- **Mock interfaces** used for NAND and Host layers.

### Testing Strategy

| Level | Example Test |
|--------|---------------|
| Unit | FTL mapping and overwrite behavior |
| Integration | Firmware issuing NAND commands |
| System | fio workloads with expected latency/throughput |
| Regression | Compare CSV traces to golden runs |

### Example GoogleTest Case

```cpp
TEST(FTLTest, WriteThenReadMapsSamePage) {
    FTL ftl(16, 64);
    auto p1 = ftl.map_write(100);
    auto p2 = ftl.map_read(100);
    EXPECT_EQ(p1.block, p2.block);
    EXPECT_EQ(p1.page, p2.page);
}
```

## Tooling & Supporting Scripts

All external tools and automation scripts are written in Python 3.

### Tool Categories

- Run automation: launch fio + simulator, collect results.
- Analysis: parse CSV/JSON logs, compute latency distributions.
- Config generation: generate NAND geometry and timing configs.
- Visualization: matplotlib-based throughput/latency plots.

### Example Tools

```text
tools/
  run_fio_sim.py        # orchestrates fio + simulator runs
  analyze_results.py    # parses logs, computes KPIs
  gen_config.py         # generates JSON configs
  plot_metrics.py       # visualizes metrics
```

### Example usage

```bash
python tools/run_fio_sim.py --config config/default.json --fio fio_jobs/randwrite.fio
python tools/analyze_results.py --input results/run_001.csv
```

## Migration Path to Zephyr & RISC-V

1. Abstract firmware to separate C++ library — libfirmware.a
2. Build firmware for Zephyr (native_posix) — Zephyr executable
3. Integrate Zephyr + DBT-RISE RISC-V — Firmware executes inside simulated CPU
4. Replace NAND interface API with MMIO — Hardware-accurate firmware interface

## Risks & Mitigation

| Risk | Mitigation |
| --- | --- |
| Simulation deadlocks | Use bounded FIFOs and timeout asserts |
| Buffer reuse by fio | Copy or pin buffers until completion |
| Timing divergence | Centralize simulation time via sc_time |
| Complexity growth | Enforce modular design and CI-based regression tests |
| fio instability | Keep ioengine isolated and minimal |

## Next Steps

1. Initialize CMake project with SystemC and GoogleTest.
2. Implement Phase 0–1:
   - fio ioengine plugin + libssdsim.so
   - SystemC HostInterface + Firmware stub
3. Set up GoogleTest + CI workflow.
4. Build Python tools for configuration and analysis.
5. Iterate inward with TDD to add NAND and FTL modules.

## Appendix: Key Dependencies

| Tool | Purpose |
| --- | --- |
| SystemC 2.3+ | Simulation and TLM infrastructure |
| GoogleTest | Unit and integration testing |
| fio | Host I/O workload generation |
| Python 3 | Tooling and analysis |
| CMake | Build configuration |
| clang-tidy / cppcheck | Static analysis |
