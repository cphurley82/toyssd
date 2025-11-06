"""Simulation configuration dataclasses.

Rationale:
- Keep Python API declarative and safe to pass across boundaries.
- Use ``frozen=True`` and ``slots=True`` to make configs immutable and compact
    (fewer attribute dicts, cheaper copies, easier to hash/cache in the future).

Configuration mirrors the design doc and intentionally stays metadata-only;
timings and patterns are modeled by the SystemC core and exposed through
events, so Python focuses on structure and validation.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Optional


@dataclass(frozen=True, slots=True)
class NandGeometry:
    """Geometry description for the NAND model.

    Rationale:
    - The geometry drives derived capacities and visualization layout but does
      not carry live state; therefore we keep it immutable.
    - Defaults target a small, quick-to-simulate device while retaining
      realistic shapes (e.g., 16 KiB pages).
    """

    capacity_gb: int = 1
    dies: int = 4
    blocks_per_die: int = 1024
    pages_per_block: int = 256
    page_size_bytes: int = 16_384
    oob_size_bytes: int = 1024
    planes_per_die: int = 2
    # TODO: add multi-level cell support.
    # TODO: decide on better handling for planes, should we use block number to
    #    determine plane or make planes first-class in the geometry?

    @property
    def total_blocks(self) -> int:
        """Total blocks across all dies.

        Provided as a convenience so callers don't re-implement the math.
        """
        return self.dies * self.blocks_per_die

    @property
    def pages_per_die_total(self) -> int:
        """Total pages in a single die (blocks × pages per block)."""
        return self.blocks_per_die * self.pages_per_block

    @property
    def block_size_bytes(self) -> int:
        """Size of one block in bytes (pages per block × page size)."""
        return self.pages_per_block * self.page_size_bytes

    @property
    def total_capacity_bytes(self) -> int:
        """Raw capacity in bytes based on configured geometry.

        Why raw: over-provisioning and FTL metadata are modeled at higher
        layers; geometry only describes the physical array shape.
        """
        return (
            self.dies
            * self.blocks_per_die
            * self.pages_per_block
            * self.page_size_bytes
        )


@dataclass(slots=True)
class SimConfig:
    """Runtime configuration for the ToySSD simulation.

    Rationale:
    - Group all tunables in one object to simplify API evolution.
    - Keep ``slots=True`` to reduce overhead during frequent access in
      orchestrator loops.
    - Validation in ``__post_init__`` fails fast on misconfiguration to avoid
      hard-to-debug runtime errors later in SystemC.
    """

    nand_geometry: NandGeometry
    enable_visualization: bool = True
    log_level: str = "INFO"
    event_buffer_capacity: int = 1024
    time_step_ms: int = 1
    host_queue_depth: int = 1
    seed: Optional[int] = None

    def __post_init__(self) -> None:
        """Validate user-provided values early with friendly messages.

        We prefer explicit validation here rather than implicit failures deep
        in the simulation flow; it shortens the feedback cycle for users and
        tests.
        """
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
