# Two layers in one process tree: the host layer builds a pinned virtual environment and
# re-executes this same file inside it, because upstream xacro must never be importable from the
# interpreter a developer happens to run.

import os
import sys
import argparse
import subprocess
from pathlib import Path

from recorder.corpus import REPO
from recorder.records import measure
from recorder.environment import bootstrap, measured_pins, package_roots

HERE = Path(__file__).resolve().parent


def worker(out):
    try:
        import ament_index_python.packages  # noqa: F401
    except ImportError:
        sys.exit("oracle: the vendored package index is not on PYTHONPATH; nothing was recorded")
    measure(out, measured_pins())


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--package-root", action="append", default=[])
    parser.add_argument("--out", default=str(REPO / "tests" / "golden" / "oracle"))
    parser.add_argument("--venv", default=str(REPO / "build" / "oracle-venv"))
    parser.add_argument("--worker", action="store_true")
    args = parser.parse_args()
    if args.worker:
        return worker(Path(args.out))
    if not args.package_root:
        parser.error("--package-root names the directory the descriptions resolve against")
    environment = dict(os.environ)
    # Every import-path entry precedes the environment's own site-packages, and an empty entry --
    # which is what joining an unset inherited value yields -- is the working directory.
    environment["PYTHONPATH"] = str(HERE)
    environment.pop("PYTHONHOME", None)
    environment["ORACLE_PACKAGE_ROOTS"] = os.pathsep.join(
        str(one) for one in package_roots(args.package_root))
    python = bootstrap(Path(args.venv))
    subprocess.run([str(python), __file__, "--worker", "--out", args.out],
                   env=environment, check=True)


if __name__ == "__main__":
    main()
