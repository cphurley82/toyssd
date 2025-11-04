"""Python-facing simulation orchestrator for toyssd."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Iterable, List, Optional

from .config import NandGeometry, SimConfig
from .visualization import Visualization
from .workload import Workload, WorkloadKind


class ToySSDException(Exception):
    """Base exception for ToySSD errors."""


class SimulationError(ToySSDException):
    """Raised when the SystemC simulation fails to advance."""


class CommandError(ToySSDException):
    """Raised for invalid host command parameters or sequencing."""


class CapacityError(ToySSDException):
    """Raised when host requests exceed device capacity."""


class HardwareError(ToySSDException):
    """Raised when the simulated NAND reports a failure."""


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
    """High-level Python API entry point for the SSD simulator."""

    def __init__(self, config: SimConfig) -> None:
        self._config = config
        self._stats = ToySSDStats()
        self._events: List[dict] = []
        self._bridge = _InMemoryBridge(config.nand_geometry)
        self.viz: Optional[Visualization]
        if config.enable_visualization:
            self.viz = Visualization(config.nand_geometry)
        else:
            self.viz = None

    def run_workload(
        self, workload: Workload, duration_ms: Optional[int] = None
    ) -> None:
        """Run a workload to completion."""
        events = self._bridge.run_workload(workload)
        self._record_events(events)
        if duration_ms:
            self.step(duration_ms)

    def submit_io(self, workload: Workload) -> None:
        """Submit a workload without running it immediately."""
        events = self._bridge.queue_workload(workload)
        self._record_events(events)

    def step(self, duration_ms: int) -> None:
        """Advance the simulation by the requested wall-clock time."""
        if duration_ms <= 0:
            raise ValueError("duration_ms must be positive")
        events = self._bridge.step(duration_ms)
        self._record_events(events)

    def drain_events(self) -> list[dict]:
        events = self._events.copy()
        self._events.clear()
        return events

    def get_stats(self) -> ToySSDStats:
        return self._stats

    def shutdown(self) -> None:
        """Gracefully tear down the simulation."""
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


class _InMemoryBridge:
    """Placeholder bridge that mimics controller <-> NAND behaviour in Python."""

    def __init__(self, geometry: NandGeometry) -> None:
        self._geometry = geometry
        self._storage: dict[int, bytes] = {}
        self._pending: List[Workload] = []
        self._time_acc_ms = 0

    def queue_workload(self, workload: Workload) -> list[dict]:
        self._pending.append(workload)
        return [{"type": "queue", "workload": workload.kind.value}]

    def run_workload(self, workload: Workload) -> list[dict]:
        self._pending.append(workload)
        return self._process_pending()

    def step(self, duration_ms: int) -> list[dict]:
        self._time_acc_ms += duration_ms
        if not self._pending:
            return [{"type": "idle", "duration_ms": duration_ms}]
        return self._process_pending()

    def shutdown(self) -> None:
        self._pending.clear()

    def _process_pending(self) -> list[dict]:
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
        events: list[dict] = []
        lba = workload.start_lba
        for _ in range(workload.lba_count):
            self._storage[lba] = self._pattern_bytes(lba, workload)
            events.append({"type": "write", "lba": lba, "pattern": workload.kind.value})
            lba += 1
        return events

    def _handle_read(self, workload: Workload) -> list[dict]:
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
        block_bytes = workload.block_size_kb * 1024
        if block_bytes <= 0:
            raise CommandError("block_size_kb must be positive")
        counter = (lba + (workload.randomness_seed or 0)) & 0xFF
        return bytes([counter] * block_bytes)
