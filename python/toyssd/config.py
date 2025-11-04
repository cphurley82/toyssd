"""Simulation configuration dataclasses."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Optional


@dataclass(frozen=True, slots=True)
class NandGeometry:
    """Geometry description for the NAND model."""

    capacity_gb: int = 1
    dies: int = 4
    blocks_per_die: int = 1024
    pages_per_block: int = 256
    page_size_bytes: int = 16_384
    oob_size_bytes: int = 1024
    planes_per_die: int = 2

    @property
    def total_blocks(self) -> int:
        return self.dies * self.blocks_per_die

    @property
    def pages_per_die_total(self) -> int:
        return self.blocks_per_die * self.pages_per_block

    @property
    def block_size_bytes(self) -> int:
        return self.pages_per_block * self.page_size_bytes

    @property
    def total_capacity_bytes(self) -> int:
        return (
            self.dies
            * self.blocks_per_die
            * self.pages_per_block
            * self.page_size_bytes
        )


@dataclass(slots=True)
class SimConfig:
    """Runtime configuration for the ToySSD simulation."""

    nand_geometry: NandGeometry
    enable_visualization: bool = True
    log_level: str = "INFO"
    event_buffer_capacity: int = 1024
    time_step_ms: int = 1
    host_queue_depth: int = 1
    seed: Optional[int] = None

    def __post_init__(self) -> None:
        if self.host_queue_depth < 1:
            raise ValueError("host_queue_depth must be >= 1")
        if self.time_step_ms <= 0:
            raise ValueError("time_step_ms must be positive")
        if self.event_buffer_capacity <= 0:
            raise ValueError("event_buffer_capacity must be positive")
        valid_levels = {"DEBUG", "INFO", "WARNING", "ERROR"}
        if self.log_level.upper() not in valid_levels:
            raise ValueError(
                f"log_level must be one of {sorted(valid_levels)}; got {self.log_level}"
            )
