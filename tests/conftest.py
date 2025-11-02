from __future__ import annotations

import pathlib
import sys

_REPO_ROOT = pathlib.Path(__file__).resolve().parents[1]
_PY_DIR = _REPO_ROOT / "python"
if _PY_DIR.exists():
    sys.path.insert(0, str(_PY_DIR))
