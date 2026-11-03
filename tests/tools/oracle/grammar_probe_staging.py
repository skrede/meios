# How the reference renders a grammar probe: which probes render in their own stage and without
# the vendored package index, and the one environment every probe reads.

import os
import sys
import contextlib
from pathlib import Path

HERE = Path(__file__).resolve().parent

GRAMMAR_PROBES_IN_STAGE = ("cwd_command",)
# Rendered by the pinned install as it ships, which has no package index: the vendored one is what
# lets every other probe's find resolve.
GRAMMAR_PROBES_WITHOUT_PACKAGE_INDEX = ("find_without_package_index",)


# Both sides read this one fixed environment, so no row spells a machine's own.
def probe_environment():
    os.environ["MEIOS_PROBE_ENV"] = "probe_value"
    os.environ.pop("MEIOS_PROBE_UNSET", None)


# The vendored index is taken off the import path and out of the module cache while the probe
# renders, so upstream's own import of it fails as it does in the bare pinned install.
@contextlib.contextmanager
def without_package_index(case_id):
    if case_id not in GRAMMAR_PROBES_WITHOUT_PACKAGE_INDEX:
        yield
        return
    saved_path = list(sys.path)
    saved_modules = {name: sys.modules.pop(name) for name in list(sys.modules)
                     if name.split(".")[0] == "ament_index_python"}
    try:
        sys.path[:] = [one for one in sys.path if Path(one or ".").resolve() != HERE]
        yield
    finally:
        sys.path[:] = saved_path
        sys.modules.update(saved_modules)
