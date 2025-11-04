"""Workload specification helpers for the ToySSD host model."""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
from typing import Optional, Tuple


class WorkloadKind(str, Enum):
    SEQUENTIAL_WRITE = "sequential_write"
    SEQUENTIAL_READ = "sequential_read"
    RANDOM_WRITE = "random_write"
    RANDOM_READ = "random_read"


@dataclass(frozen=True, slots=True)
class Workload:
    """Describes a unit of work to run on the simulated host model."""

    kind: WorkloadKind
    start_lba: int
    lba_count: int
    block_size_kb: int
    queue_depth: int
    randomness_seed: Optional[int] = None

    def to_host_dict(self) -> dict[str, int | str | None]:
        """Export a dictionary consumable by the host binding."""
        return {
            "kind": self.kind.value,
            "start_lba": self.start_lba,
            "lba_count": self.lba_count,
            "block_size_kb": self.block_size_kb,
            "queue_depth": self.queue_depth,
            "randomness_seed": self.randomness_seed,
        }

    @classmethod
    def sequential_write(
        cls,
        *,
        start_lba: int,
        length_gb: float,
        block_size_kb: int,
        queue_depth: int = 1,
    ) -> "Workload":
        lba_count = cls._blocks_from_gib(length_gb, block_size_kb)
        return cls(
            kind=WorkloadKind.SEQUENTIAL_WRITE,
            start_lba=start_lba,
            lba_count=lba_count,
            block_size_kb=block_size_kb,
            queue_depth=queue_depth,
        )

    @classmethod
    def sequential_read(
        cls,
        *,
        start_lba: int,
        length_gb: float,
        block_size_kb: int,
        queue_depth: int = 1,
    ) -> "Workload":
        lba_count = cls._blocks_from_gib(length_gb, block_size_kb)
        return cls(
            kind=WorkloadKind.SEQUENTIAL_READ,
            start_lba=start_lba,
            lba_count=lba_count,
            block_size_kb=block_size_kb,
            queue_depth=queue_depth,
        )

    @classmethod
    def random_write(
        cls,
        *,
        lba_range: Tuple[int, int],
        io_count: int,
        block_size_kb: int,
        queue_depth: int = 1,
        randomness_seed: Optional[int] = None,
    ) -> "Workload":
        return cls(
            kind=WorkloadKind.RANDOM_WRITE,
            start_lba=lba_range[0],
            lba_count=max(io_count, 1),
            block_size_kb=block_size_kb,
            queue_depth=queue_depth,
            randomness_seed=randomness_seed,
        )

    @classmethod
    def random_read(
        cls,
        *,
        lba_range: Tuple[int, int],
        io_count: int,
        block_size_kb: int,
        queue_depth: int = 1,
        randomness_seed: Optional[int] = None,
    ) -> "Workload":
        return cls(
            kind=WorkloadKind.RANDOM_READ,
            start_lba=lba_range[0],
            lba_count=max(io_count, 1),
            block_size_kb=block_size_kb,
            queue_depth=queue_depth,
            randomness_seed=randomness_seed,
        )

    @staticmethod
    def _blocks_from_gib(length_gb: float, block_size_kb: int) -> int:
        if block_size_kb <= 0:
            raise ValueError("block_size_kb must be positive")
        if length_gb < 0:
            raise ValueError("length_gb must be non-negative")
        bytes_total = length_gb * (1024**3)
        bytes_per_block = block_size_kb * 1024
        return int(bytes_total // bytes_per_block)
