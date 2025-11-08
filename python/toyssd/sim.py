"""Python-facing simulation orchestrator for toyssd.

Responsibilities:
- Provides a clean, synchronous API for advancing the SystemC kernel from
    Python without threads. SystemC requires single-threaded semantics; driving
    ``sc_start`` in controlled steps keeps determinism and makes tests simple.
- Aggregates stats and visualization updates so callers don't need to poll or
    coordinate components manually.
- Uses a metadata-only contract: we exchange command parameters and event
    records, not page data buffers. This makes the simulator memory-light and
    fast to iterate while still validating controller/NAND logic.

Note: The current implementation routes through an in-memory Python bridge to
stand in for the C++ SystemC core while the bindings are developed. The
public API is designed to remain stable when the bridge is replaced by real
bindings.
TODO(cphurley): Update this doc when the SystemC bridge is implemented.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Iterable, List, Optional

from .config import NandGeometry, SimConfig
from .visualization import Visualization
from .workload import Workload, WorkloadKind


class ToySSDException(Exception):
    """Base exception for ToySSD errors.

    A common base type lets client code catch all simulator errors if it
    doesn't care to distinguish between configuration, capacity, or hardware
    failures. Specific subclasses carry clearer intent where needed.
    """


class SimulationError(ToySSDException):
    """Raised when the SystemC simulation fails to advance.

    In a real SystemC-backed run this would wrap non-OK TLM responses or kernel
    errors. It remains for API completeness in the Python bridge.
    TODO(cphurley): Update this doc when the SystemC bridge is implemented.
    """


class CommandError(ToySSDException):
    """Raised for invalid host command parameters or sequencing.

    We prefer failing fast on invalid input rather than silently clamping or
    guessing, to preserve determinism and test clarity.
    """


class CapacityError(ToySSDException):
    """Raised when host requests exceed device capacity.

    For example, reading an LBA that hasn't been written in the model
    simulates a capacity/provisioning violation.
    """


class HardwareError(ToySSDException):
    """Raised when the simulated NAND reports a failure.

    Used for verification mismatches to model NAND status failure codes.
    """


@dataclass(slots=True)
class ToySSDStats:
    total_writes: int = 0
    total_reads: int = 0
    total_erases: int = 0

    @property
    def write_amplification(self) -> float:
        if self.total_writes == 0:
            return 0.0
        # Direct map skeleton keeps amplification at 1.0 for now.
        return 1.0


class ToySSD:
    """High-level Python API entry point for the SSD simulator.

    Responsibilities:
    - Hold configuration, stats, and an events queue.
    - Own the visualization instance and feed it events.
    - Delegate execution to the selected backend bridge (in-memory or SystemC).

    Backend selection:
    The ``SimConfig.backend`` field chooses the implementation:
    - ``"python"`` (default): fast, dependency-free in-memory bridge for
      development and CI.
    - ``"systemc"``: native SystemC/PySysC model once bindings are available.
      If requested but unavailable at runtime, a clear ``SimulationError`` is
      raised.
    """

    def __init__(self, config: SimConfig) -> None:
        self._config = config
        self._stats = ToySSDStats()
        self._events = []
        if config.backend == "python":
            self._bridge = _InMemoryBridge(config.nand_geometry)
        else:
            self._bridge = _SystemCBridge(config.nand_geometry)
        self.viz = (
            Visualization(config.nand_geometry) if config.enable_visualization else None
        )

    def run_workload(
        self, workload: Workload, duration_ms: Optional[int] = None
    ) -> None:
        """Run a workload to completion.

        Advance time afterwards in a single step to emulate a synchronous
        "run then settle" phase.
        """
        events = self._bridge.run_workload(workload)
        self._record_events(events)
        if duration_ms:
            self.step(duration_ms)

    def submit_io(self, workload: Workload) -> None:
        """Submit a workload without running it immediately.

        Models queuing on the host side when the caller wants explicit
        control over when execution occurs.
        """
        events = self._bridge.queue_workload(workload)
        self._record_events(events)

    def step(self, duration_ms: int) -> None:
        """Advance the simulation by the requested wall-clock time.

        We validate that ``duration_ms`` is positive to avoid no-op advances
        and keep logs meaningful.
        """
        if duration_ms <= 0:
            raise ValueError("duration_ms must be positive")
        events = self._bridge.step(duration_ms)
        self._record_events(events)

    def drain_events(self) -> list[dict]:
        """Return and clear accumulated events since the last drain.

        Keeping events in Python space decouples visualization/logging from the
        execution loop and mirrors how vector-based buffers are drained from
        the C++ side.
        """
        events = self._events.copy()
        self._events.clear()
        return events

    def get_stats(self) -> ToySSDStats:
        """Return cumulative simulator statistics."""
        return self._stats

    def shutdown(self) -> None:
        """Gracefully tear down the simulation.

        Release native resources and join kernel state.
        """
        self._bridge.shutdown()

    def _record_events(self, events: Iterable[dict]) -> None:
        for event in events:
            self._events.append(event)
            if event["type"] == "write":
                self._stats.total_writes += 1
            elif event["type"] == "read":
                self._stats.total_reads += 1
            elif event["type"] == "erase":
                self._stats.total_erases += 1
        if self.viz:
            self.viz.update(events)


class _BridgeProtocol:  # Lightweight structural protocol (no typing import for minimal deps)
    """Informal protocol documenting required bridge methods."""

    def queue_workload(self, workload: Workload) -> list[dict]: ...  # noqa: D401,E701
    def run_workload(self, workload: Workload) -> list[dict]: ...  # noqa: D401,E701
    def step(self, duration_ms: int) -> list[dict]: ...  # noqa: D401,E701
    def shutdown(self) -> None: ...  # noqa: D401,E701


class _InMemoryBridge(_BridgeProtocol):
    """Placeholder bridge that mimics controller <-> NAND behaviour in Python.

    Enables end-to-end API and visualization development before native bindings
    are available, and provides a reference for unit tests that don't depend on
    SystemC being present.
    """

    def __init__(self, geometry: NandGeometry) -> None:
        self._geometry = geometry
        self._storage: dict[int, bytes] = {}
        self._pending: List[Workload] = []
        self._time_acc_ms = 0

    def queue_workload(self, workload: Workload) -> list[dict]:
        """Queue a workload for later processing and emit a queue event."""
        self._pending.append(workload)
        return [{"type": "queue", "workload": workload.kind.value}]

    def run_workload(self, workload: Workload) -> list[dict]:
        """Run a single workload immediately and return resulting events."""
        self._pending.append(workload)
        return self._process_pending()

    def step(self, duration_ms: int) -> list[dict]:
        """Advance simulated time and process any pending work.

        We accumulate "time" locally to keep the method pure from the
        perspective of SystemC integration (where this will call ``sc_start``).
        """
        self._time_acc_ms += duration_ms
        if not self._pending:
            return [{"type": "idle", "duration_ms": duration_ms}]
        return self._process_pending()

    def shutdown(self) -> None:
        """Drop any queued work and reset ephemeral bridge state."""
        self._pending.clear()

    def _process_pending(self) -> list[dict]:
        """Process all queued workloads and return a flat list of events."""
        events: list[dict] = []
        while self._pending:
            workload = self._pending.pop(0)
            if workload.kind in (
                WorkloadKind.SEQUENTIAL_WRITE,
                WorkloadKind.RANDOM_WRITE,
            ):
                events.extend(self._handle_write(workload))
            else:
                events.extend(self._handle_read(workload))
        return events

    def _handle_write(self, workload: Workload) -> list[dict]:
        """Simulate writing blocks by storing a compact pattern per LBA."""
        events: list[dict] = []
        lba = workload.start_lba
        for _ in range(workload.lba_count):
            self._storage[lba] = self._pattern_bytes(lba, workload)
            events.append({"type": "write", "lba": lba, "pattern": workload.kind.value})
            lba += 1
        return events

    def _handle_read(self, workload: Workload) -> list[dict]:
        """Verify reads by comparing against the expected pattern bytes."""
        events: list[dict] = []
        lba = workload.start_lba
        for _ in range(workload.lba_count):
            data = self._storage.get(lba)
            if data is None:
                raise CapacityError(f"LBA {lba} not written before read")
            expected = self._pattern_bytes(lba, workload)
            if data != expected:
                raise HardwareError(f"LBA {lba} verification failed")
            events.append({"type": "read", "lba": lba, "pattern": workload.kind.value})
            lba += 1
        return events

    def _pattern_bytes(self, lba: int, workload: Workload) -> bytes:
        """Create a deterministic byte pattern for a given LBA/workload.

        We avoid moving large data buffers and still check read correctness. The
        pattern is a single repeated byte derived from LBA and an optional seed
        to keep it simple and cheap.
        """
        block_bytes = workload.block_size_kb * 1024
        if block_bytes <= 0:
            raise CommandError("block_size_kb must be positive")
        counter = (lba + (workload.randomness_seed or 0)) & 0xFF
        return bytes([counter] * block_bytes)


class _SystemCBridge(_BridgeProtocol):
    """Placeholder for the future SystemC-backed bridge.

    Raises a clear error today so users understand the backend isn't wired
    yet rather than silently falling back.
    """

    def __init__(self, geometry: NandGeometry) -> None:  # noqa: D401
        raise SimulationError(
            "SystemC backend requested but native bindings are not yet available."
        )

    def queue_workload(self, workload: Workload) -> list[dict]:  # pragma: no cover
        return []

    def run_workload(self, workload: Workload) -> list[dict]:  # pragma: no cover
        return []

    def step(self, duration_ms: int) -> list[dict]:  # pragma: no cover
        return []

    def shutdown(self) -> None:  # pragma: no cover
        pass
