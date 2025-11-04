"""Simple visualization placeholder inspired by Norton Disk Doctor."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, List

from .config import NandGeometry


@dataclass(slots=True)
class BlockState:
    lba: int
    state: str


class Visualization:
    """ASCII-based visualization placeholder for NAND state."""

    def __init__(self, geometry: NandGeometry) -> None:
        self._geometry = geometry
        self._history: List[str] = []
        self._state: dict[int, BlockState] = {}

    def update(self, events: Iterable[dict]) -> None:
        for event in events:
            event_type = event.get("type")
            if event_type not in {"write", "read", "erase"}:
                continue
            lba = int(event["lba"])
            self._state[lba] = BlockState(lba=lba, state=event_type)
            self._history.append(f"{event_type.upper()} lba={lba}")

    def render(self) -> str:
        if not self._state:
            return "<no activity>"
        rows = []
        for lba in sorted(self._state.keys()):
            state = self._state[lba].state
            glyph = self._glyph_for_state(state)
            rows.append(f"{lba:06d}:{glyph}")
        return "\n".join(rows)

    def save_snapshot(self, path: str | Path) -> None:
        Path(path).write_text(self.render(), encoding="utf-8")

    def history(self) -> List[str]:
        return list(self._history)

    @staticmethod
    def _glyph_for_state(state: str) -> str:
        if state == "write":
            return "[#]"
        if state == "read":
            return "[=]"
        if state == "erase":
            return "[ ]"
        return "[?]"
