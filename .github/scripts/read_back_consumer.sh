#!/usr/bin/env bash
# usage: read_back_consumer.sh <staged-install-prefix> <consumer-build-tree>...
#
# Reads the built consumer executables and the staged public headers back, and refuses if either
# carries what the build claims it does not.
set -u

if [ "$#" -lt 2 ]; then
    echo "usage: $(basename "$0") <staged-install-prefix> <consumer-build-tree>..." >&2
    exit 2
fi

install="$1"
shift
status=0

refuse()
{
    echo "$1"
    status=1
}

# Two stages on purpose, here and below. A count taken straight out of a pipeline reports zero for
# a producer that could not run at all, and a zero is exactly what a clean subject reports, so the
# producing command's own exit status is read before anything counts its output.
read_probe()
{
    probe="$1"
    if symbols=$(nm -C "$probe" 2>/dev/null); then
        carried=$(printf '%s\n' "$symbols" | grep -cE 'Py_Initialize|pybind11' || true)
        test "$carried" -eq 0 || refuse "$probe carries an interpreter symbol"
    else
        refuse "$probe yielded no symbol table, so nothing was read back from it"
    fi
    if linkage=$(ldd "$probe" 2>/dev/null); then
        linked=$(printf '%s\n' "$linkage" | grep -ci python || true)
        test "$linked" -eq 0 || refuse "$probe links an interpreter shared object"
    else
        refuse "$probe yielded no dynamic linkage, so nothing was read back from it"
    fi
}

for tree in "$@"; do
    if ! test -d "$tree"; then
        refuse "$tree is not a directory, so the consumer was never built there"
        continue
    fi
    for probe_name in universal_robots_probe kr6_probe; do
        probe="$tree/$probe_name"
        if ! test -f "$probe"; then
            refuse "$probe is not a regular file, so $tree built no consumer to read back"
            continue
        fi
        read_probe "$probe"
    done
done

# meios's own subdirectory rather than the whole prefix: a fetched dependency that installs itself
# deposits its headers beside meios's, and a match on one of those says nothing at all about what
# meios exposes.
headers="$install/include/meios"
if test -d "$headers"; then
    named=$(grep -rl 'yaml-cpp/\|YAML::' "$headers" 2>/dev/null)
    searched=$?
    if test "$searched" -gt 1; then
        refuse "$headers could not be searched, so no header was read back"
    elif test -n "$named"; then
        refuse "an installed public header names the auxiliary-format module's own dependency:"
        echo "$named"
    fi
else
    refuse "$headers is not a directory, so the install staged no public headers"
fi

exit $status
