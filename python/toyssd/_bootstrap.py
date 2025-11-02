from __future__ import annotations

import contextlib
import ctypes
import importlib.util
import os
import pathlib
import shutil
import subprocess
import sys
from collections.abc import Iterable, Sequence
from dataclasses import dataclass

_PKG_DIR = pathlib.Path(__file__).resolve().parent
_VENDOR_DIR = _PKG_DIR / "_vendor"
_NATIVE_DIR = _PKG_DIR / "_native"
_LIB_DIR = _PKG_DIR.parent / "lib"


def _prepend_env_paths(var: str, paths: Sequence[pathlib.Path]) -> None:
    existing = os.environ.get(var, "")
    parts = [str(p) for p in paths if p and p.exists()]
    if not parts:
        return
    if existing:
        parts.extend(x for x in existing.split(os.pathsep) if x)
    dedup: list[str] = []
    seen = set()
    for item in parts:
        if item not in seen:
            dedup.append(item)
            seen.add(item)
    os.environ[var] = os.pathsep.join(dedup)


def _ensure_vendor_on_sys_path() -> None:
    if _VENDOR_DIR.exists():
        vendor_str = str(_VENDOR_DIR)
        if vendor_str not in sys.path:
            sys.path.insert(0, vendor_str)


def _resolve_systemc_root() -> pathlib.Path | None:
    vendor_root = _VENDOR_DIR / "systemc"
    if vendor_root.exists():
        return vendor_root
    candidates = [
        (_PKG_DIR.parent / ".." / "build" / "_deps" / "systemc").resolve(),
        (_PKG_DIR.parent / ".." / "build-debug" / "_deps" / "systemc").resolve(),
        (_PKG_DIR.parent / ".." / "build-release" / "_deps" / "systemc").resolve(),
    ]
    for cand in candidates:
        if cand.exists():
            return cand
    return None


def _preload_systemc() -> None:
    systemc_root = _SYSTEMC_ROOT
    if systemc_root is None:
        return
    os.environ.setdefault("SYSTEMC_HOME", str(systemc_root))
    lib_dir = systemc_root / "lib"
    include_dir = systemc_root / "include"
    _prepend_env_paths("LD_LIBRARY_PATH", [lib_dir, _NATIVE_DIR, _LIB_DIR])
    if sys.platform == "darwin":
        _prepend_env_paths("DYLD_LIBRARY_PATH", [lib_dir, _NATIVE_DIR, _LIB_DIR])
        _prepend_env_paths("DYLD_FALLBACK_LIBRARY_PATH", [lib_dir])

    candidates: Iterable[pathlib.Path] = []
    if lib_dir.exists():
        candidates = list(lib_dir.glob("libsystemc*.so")) + list(lib_dir.glob("libsystemc*.dylib"))
    for cand in candidates:
        try:
            ctypes.CDLL(str(cand), mode=ctypes.RTLD_GLOBAL)
            break
        except OSError:
            continue

    os.environ.setdefault("TOYSSD_SYSTEMC_INCLUDE", str(include_dir))


_SYSTEMC_ROOT = _resolve_systemc_root()

_ensure_vendor_on_sys_path()
_preload_systemc()


def _prepare_cppyy() -> None:
    if sys.platform != "darwin":
        return
    spec = importlib.util.find_spec("cppyy_backend")
    if not spec or not spec.origin:
        return
    backend_lib = pathlib.Path(spec.origin).resolve().parent / "lib"
    _prepend_env_paths("DYLD_LIBRARY_PATH", [backend_lib])
    _prepend_env_paths("DYLD_FALLBACK_LIBRARY_PATH", [backend_lib])

    cling_path = backend_lib / "libCling.so"
    zstd_target = backend_lib / "libzstd.1.dylib"
    zstd_sources = [
        pathlib.Path("/opt/homebrew/opt/zstd/lib/libzstd.1.dylib"),
        pathlib.Path("/usr/local/opt/zstd/lib/libzstd.1.dylib"),
    ]
    copied_zstd: pathlib.Path | None = None
    if not zstd_target.exists():
        for source in zstd_sources:
            if source.exists():
                try:
                    shutil.copy2(source, zstd_target)
                    copied_zstd = source
                except OSError:
                    pass
                break
    else:
        copied_zstd = zstd_target
    if cling_path.exists() and copied_zstd is not None:
        with contextlib.suppress(FileNotFoundError):
            subprocess.run(
                [
                    "install_name_tool",
                    "-change",
                    "/opt/local/lib/libzstd.1.dylib",
                    str(copied_zstd),
                    str(cling_path),
                ],
                check=False,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
    for candidate in [zstd_target, *zstd_sources]:
        if candidate.exists():
            with contextlib.suppress(OSError):
                ctypes.CDLL(str(candidate), mode=ctypes.RTLD_GLOBAL)
                break
    libcppyy_backend = backend_lib / "libcppyy_backend.so"
    if libcppyy_backend.exists():
        with contextlib.suppress(OSError):
            ctypes.CDLL(str(libcppyy_backend), mode=ctypes.RTLD_GLOBAL)


_prepare_cppyy()

import cppyy  # noqa: E402,I001  # pylint: disable=wrong-import-position
import pysysc  # noqa: E402,F401,I001  # pylint: disable=wrong-import-position


os.environ.setdefault("EXTRA_CLING_ARGS", "-std=c++20")

_CPP_INCLUDE_CANDIDATES = [
    _PKG_DIR.parent / "include",
]
if _SYSTEMC_ROOT is not None:
    _CPP_INCLUDE_CANDIDATES.append(_SYSTEMC_ROOT / "include")
for path in _CPP_INCLUDE_CANDIDATES:
    if path.exists():
        cppyy.add_include_path(str(path))


_LIB_DIR_CANDIDATES = [
    _NATIVE_DIR,
    _LIB_DIR,
    (_PKG_DIR.parent / ".." / "build" / "lib").resolve(),
    (_PKG_DIR.parent / ".." / "build-debug" / "lib").resolve(),
    (_PKG_DIR.parent / ".." / "build-release" / "lib").resolve(),
]


def _load_library(name: str) -> None:
    patterns = [
        f"lib{name}.so",
        f"{name}.so",
        f"lib{name}.dylib",
        f"{name}.dylib",
        f"{name}.dll",
    ]
    for directory in _LIB_DIR_CANDIDATES:
        if not directory.exists():
            continue
        for pattern in patterns:
            for candidate in directory.glob(pattern):
                cppyy.load_library(str(candidate))
                return
    cppyy.load_library(name)


def _load_with_ctypes(name: str) -> bool:
    patterns = [
        f"lib{name}.so",
        f"{name}.so",
        f"lib{name}.dylib",
        f"{name}.dylib",
    ]
    for directory in _LIB_DIR_CANDIDATES:
        if not directory.exists():
            continue
        for pattern in patterns:
            for candidate in directory.glob(pattern):
                try:
                    target = _NATIVE_DIR / candidate.name
                    if candidate.parent != _NATIVE_DIR and candidate.exists():
                        try:
                            _NATIVE_DIR.mkdir(parents=True, exist_ok=True)
                            shutil.copy2(candidate, target)
                            candidate = target
                        except OSError:
                            pass
                    ctypes.CDLL(str(candidate), mode=ctypes.RTLD_GLOBAL)
                    return True
                except OSError:
                    continue
    return False


_load_with_ctypes("scmain_stub")
_load_library("scmain_stub")
_load_library("toyssd_pycore")

cppyy.include("toyssd/sim/python/ToyssdPyAdapter.h")

gbl = cppyy.gbl

_OP_STR = {
    0: "READ",
    1: "PROGRAM",
    2: "ERASE",
}


@dataclass(slots=True)
class Completion:
    request_id: int
    status: int
    completion_ns: int


@dataclass(slots=True)
class NandAddress:
    channel: int
    ce: int
    lun: int
    plane: int
    block: int
    wordline: int
    logical_page: int


@dataclass(slots=True)
class NandEvent:
    op: str
    address: NandAddress
    time_ps: int


class ToySSD:
    """High-level Python façade over toyssd::python::ToyssdPyAdapter."""

    def __init__(self) -> None:
        self._adapter = gbl.toyssd.python.ToyssdPyAdapter()

    def reset(self) -> None:
        self._adapter.reset()

    def submit_write(self, lba: int, size_bytes: int = 4096) -> int:
        return int(self._adapter.submit_write(int(lba), int(size_bytes)))

    def submit_read(self, lba: int, size_bytes: int = 4096) -> int:
        return int(self._adapter.submit_read(int(lba), int(size_bytes)))

    def run_for(
        self, *, ns: int | None = None, us: int | None = None, ms: int | None = None
    ) -> None:
        if ns is not None:
            self._adapter.run_for_ns(int(ns))
        elif us is not None:
            self._adapter.run_for_us(int(us))
        elif ms is not None:
            self._adapter.run_for_ms(int(ms))

    def time_ps(self) -> int:
        return int(self._adapter.time_ps())

    def poll(self, max_completions: int = 16) -> list[Completion]:
        results = []
        vec = self._adapter.poll(int(max_completions))
        for item in vec:
            results.append(
                Completion(
                    request_id=int(item.request_id),
                    status=int(item.status),
                    completion_ns=int(item.completion_ns),
                )
            )
        return results

    def drain_nand_events(self) -> list[NandEvent]:
        events = []
        vec = self._adapter.drain_nand_events()
        for event in vec:
            op_val = int(event.op)
            addr = event.addr
            events.append(
                NandEvent(
                    op=_OP_STR.get(op_val, f"UNKNOWN({op_val})"),
                    address=NandAddress(
                        channel=int(addr.channel),
                        ce=int(addr.ce),
                        lun=int(addr.lun),
                        plane=int(addr.plane),
                        block=int(addr.block),
                        wordline=int(addr.wordline),
                        logical_page=int(addr.logical_page),
                    ),
                    time_ps=int(event.time_ps),
                )
            )
        return events
