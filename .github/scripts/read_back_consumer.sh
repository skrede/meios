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

plat=$(uname -s)
case "$plat" in
    Linux*) plat=linux ;;
    Darwin*) plat=darwin ;;
    MINGW*|MSYS*|CYGWIN*) plat=windows ;;
esac

# Two stages on purpose, here and below. A count taken straight out of a pipeline reports zero for
# a producer that could not run at all, and a zero is exactly what a clean subject reports, so the
# producing command's own exit status is read before anything counts its output. The Windows leg is
# the exception: grep itself is the tool running the check, not an external tool whose absence would
# need detecting, so a zero-match count is the clean result there, not an ambiguous one. The PE
# image's raw bytes are read directly, needing no MSVC developer environment a bash step does not
# carry.
dll_pattern='python3[0-9]{1,3}\.dll|python[0-9]\.[0-9]+\.dll'

# A negative check that cannot fail proves nothing. The Windows leg reads raw PE bytes, so the
# pattern is first held against a subject known to carry the linkage -- the running interpreter's
# own executable, whose import table names its pythonXY.dll -- and a pattern matching nothing there
# is reported as a blind detector rather than as a clean subject. A missing reference is a refusal,
# not a skip, so the leg cannot quietly downgrade to the unproven check.
prove_interpreter_pattern()
{
    reference=$(command -v python 2>/dev/null || command -v python3 2>/dev/null)
    # command -v reports the name a shell would execute, which under MSYS resolves an .exe the
    # bare path does not name; reading the image needs the name on disk.
    if [ -n "$reference" ] && [ ! -f "$reference" ] && [ -f "$reference.exe" ]; then
        reference="$reference.exe"
    fi
    if [ -z "$reference" ] || [ ! -f "$reference" ]; then
        refuse "no interpreter executable was found to prove the read-back pattern against"
        return
    fi
    seen=$(grep -aoiE "$dll_pattern" "$reference" 2>/dev/null | wc -l)
    test "$seen" -gt 0 \
        || refuse "the interpreter-DLL pattern matched nothing in $reference, so this read-back is blind"
}

read_probe()
{
    probe="$1"
    if [ "$plat" = "windows" ]; then
        carried=$(grep -aoE 'Py_Initialize|pybind11' "$probe" 2>/dev/null | wc -l)
        test "$carried" -eq 0 || refuse "$probe carries an interpreter symbol"
        linked=$(grep -aoiE "$dll_pattern" "$probe" 2>/dev/null | wc -l)
        test "$linked" -eq 0 || refuse "$probe carries an interpreter DLL import"
        return
    fi
    if symbols=$(nm -C "$probe" 2>/dev/null); then
        carried=$(printf '%s\n' "$symbols" | grep -cE 'Py_Initialize|pybind11' || true)
        test "$carried" -eq 0 || refuse "$probe carries an interpreter symbol"
    else
        refuse "$probe yielded no symbol table, so nothing was read back from it"
    fi
    if [ "$plat" = "darwin" ]; then
        if linkage=$(otool -L "$probe" 2>/dev/null); then
            linked=$(printf '%s\n' "$linkage" | grep -ci python || true)
            test "$linked" -eq 0 || refuse "$probe links an interpreter shared object"
        else
            refuse "$probe yielded no dynamic linkage, so nothing was read back from it"
        fi
    else
        if linkage=$(ldd "$probe" 2>/dev/null); then
            linked=$(printf '%s\n' "$linkage" | grep -ci python || true)
            test "$linked" -eq 0 || refuse "$probe links an interpreter shared object"
        else
            refuse "$probe yielded no dynamic linkage, so nothing was read back from it"
        fi
    fi
}

# The staged prefix and the working tree route both build to a bare path on Linux/macOS and to
# Release\<name>.exe on Windows; a probe missing from a tree still refuses by name rather than
# aborting the loop, so the other probe in the same tree is still checked.
locate_probe()
{
    tree="$1"
    name="$2"
    if test -f "$tree/$name"; then
        printf '%s' "$tree/$name"
    elif test -f "$tree/Release/$name.exe"; then
        printf '%s' "$tree/Release/$name.exe"
    else
        return 1
    fi
}

if [ "$plat" = "windows" ]; then
    prove_interpreter_pattern
fi

for tree in "$@"; do
    if ! test -d "$tree"; then
        refuse "$tree is not a directory, so the consumer was never built there"
        continue
    fi
    for probe_name in universal_robots_probe kr6_probe; do
        if ! probe=$(locate_probe "$tree" "$probe_name"); then
            refuse "$tree built no consumer named $probe_name to read back"
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
