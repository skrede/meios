#!/usr/bin/env python3
"""Writes the Unicode identifier and normalization tables the core reads, and holds the C++ that
reads them to the interpreter over every string of the agreement domains.

Needs the pinned CPython and nothing else: no virtual environment, no xacro.
"""
import sys
import argparse
import unicodedata
from pathlib import Path

import unicode_tables
import identifier_agreement

INTERPRETER = (3, 14)
UNICODE = "16.0.0"


# The tables are this interpreter's own data, the data upstream's identifier rules run on, so any
# other minor version or Unicode version would write a different rule.
def pinned_interpreter():
    have = "{}.{}".format(*sys.version_info[:2])
    if tuple(sys.version_info[:2]) != INTERPRETER or unicodedata.unidata_version != UNICODE:
        sys.exit("unicode: this runs under Python {} with Unicode {}, where {}.{} with Unicode {} is pinned;"
                 " nothing was written".format(have, unicodedata.unidata_version, *INTERPRETER, UNICODE))


def write(out):
    out.mkdir(parents=True, exist_ok=True)
    for name, text, counts in unicode_tables.generated():
        with open(out / name, "w", encoding="ascii", newline="\n") as stream:
            stream.write(text)
        print(name, " ".join("{}={}".format(table, count) for table, count in counts))


def arguments(argv):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--out", type=Path, help="directory the generated sources are written into")
    mode.add_argument("--check-probe", type=Path, help="identifier probe executable to hold to the interpreter")
    return parser.parse_args(argv)


def main(argv=None):
    args = arguments(argv)
    pinned_interpreter()
    if args.check_probe:
        return identifier_agreement.check(args.check_probe)
    write(args.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
