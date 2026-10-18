import os
import sys
import importlib
import subprocess
from pathlib import Path
from importlib import metadata

PINS = (("xacro", "2.1.1"), ("pyyaml", "6.0.3"))
MODULES = {"xacro": "xacro", "pyyaml": "yaml"}
TOOLING = ("pip", "setuptools", "wheel")


def interpreter(venv):
    return venv / ("Scripts/python.exe" if os.name == "nt" else "bin/python")


def distributions(python):
    listing = subprocess.run([str(python), "-m", "pip", "list", "--format=freeze"],
                             capture_output=True, text=True, check=True).stdout
    return dict(line.lower().split("==", 1) for line in listing.split() if "==" in line)


def bootstrap(venv):
    python = interpreter(venv)
    if not python.exists():
        subprocess.run([sys.executable, "-m", "venv", str(venv)], check=True)
    for name, want in PINS:
        have = distributions(python).get(name)
        if have and have != want:
            sys.exit("oracle: {} {} is installed where {} is pinned; delete {} and rerun"
                     .format(name, have, want, venv))
    subprocess.run([str(python), "-m", "pip", "install", "--quiet", "--disable-pip-version-check",
                    *("{}=={}".format(n, v) for n, v in PINS)], check=True)
    extra = set(distributions(python)) - set(n for n, _ in PINS) - set(TOOLING)
    if extra:
        sys.exit("oracle: resolution pulled unpinned distributions: " + " ".join(sorted(extra)))
    return python


def package_roots(given):
    roots = [Path(one).resolve() for one in given]
    # A fetched upstream lands in the build tree's resource directory beside the package-root copy
    # the corpus makes, so one root named by hand also admits the trees fetched alongside it.
    for root in list(roots):
        roots += sorted(p for p in (root.parent / "_meios_resources").glob("*") if p.is_dir())
    return roots



def imported_from_pin(module_name):
    origin = Path(importlib.import_module(module_name).__file__).resolve()
    if not origin.is_relative_to(Path(sys.prefix).resolve()):
        sys.exit("oracle: {} was imported from {}, outside the environment at {}; nothing was "
                 "recorded".format(module_name, origin, sys.prefix))


# The record's claim that exactly these distributions produced every measurement below it is a fact
# only if the run reads the versions out of the environment it measures in.
def measured_pins():
    versions = {}
    for name, want in PINS:
        imported_from_pin(MODULES[name])
        have = metadata.version(name)
        if have != want:
            sys.exit("oracle: the environment holds {} {} where {} is pinned; nothing was recorded"
                     .format(name, have, want))
        versions[name] = have
    return versions

