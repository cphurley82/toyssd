"""Top-level Python API for the toyssd simulator.

Exports a compact, Python-first surface area for constructing and driving the
SSD simulation from user code or tests. The API intentionally hides the
SystemC details behind a synchronous, step-driven interface because:

- SystemC is single-threaded by design; advancing time explicitly from Python
    keeps execution deterministic and easy to reason about.
- A small set of primitives (``ToySSD``, ``SimConfig``, ``NandGeometry``,
    ``Workload``) encourages composition at the Python layer while the C++ core
    focuses on device behavior.

See ``docs/ssd_sim_design.md`` for architecture and rationale.
"""

from .config import NandGeometry, SimConfig
from .sim import ToySSD
from .workload import Workload

__all__ = [
    "ToySSD",
    "NandGeometry",
    "SimConfig",
    "Workload",
]
