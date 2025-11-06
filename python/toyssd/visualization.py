"""Simple visualization placeholder inspired by Norton Disk Doctor.

Why ASCII first:
- Keeps feedback loops tight in terminals and CI without GUI dependencies.
- Text snapshots are easy to diff and use as golden files in tests.
- The concrete rendering will evolve, but the event-driven interface remains.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, List

from .config import NandGeometry


@dataclass(slots=True)
class BlockState:
    """Compact state per LBA for rendering and history purposes."""

    lba: int
    state: str


class Visualization:
    """ASCII-based visualization placeholder for NAND state.

    Consumes events from the orchestrator and converts them into a compact
    string grid. The goal is to make internal state observable while a richer
    UI is built out.
    """

    def __init__(self, geometry: NandGeometry) -> None:
        self._geometry = geometry
        self._history: List[str] = []
        self._state: dict[int, BlockState] = {}

    def update(self, events: Iterable[dict]) -> None:
        """Apply a batch of simulator events to the internal view model."""
        for event in events:
            event_type = event.get("type")
            if event_type not in {"write", "read", "erase"}:
                continue
            lba = int(event["lba"])
            self._state[lba] = BlockState(lba=lba, state=event_type)
            self._history.append(f"{event_type.upper()} lba={lba}")

    def render(self) -> str:
        """Return a text representation of the current NAND activity/state."""
        if not self._state:
            return "<no activity>"
        rows = []
        for lba in sorted(self._state.keys()):
            state = self._state[lba].state
            glyph = self._glyph_for_state(state)
            rows.append(f"{lba:06d}:{glyph}")
        return "\n".join(rows)

    def save_snapshot(self, path: str | Path) -> None:
        """Write the current rendered state to a file on disk."""
        Path(path).write_text(self.render(), encoding="utf-8")

    def history(self) -> List[str]:
        """Return a copy of the event strings accumulated so far."""
        return list(self._history)

    @staticmethod
    def _glyph_for_state(state: str) -> str:
        """Map event state to a single-glyph visual for compact rendering."""
        if state == "write":
            return "[#]"
        if state == "read":
            return "[=]"
        if state == "erase":
            return "[ ]"
        return "[?]"
