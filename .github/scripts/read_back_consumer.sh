#!/usr/bin/env bash
# usage: read_back_consumer.sh <staged-install-prefix> <consumer-build-tree>...
#
# Reads the built consumer executables and the staged public headers back, and refuses if either
# carries what the build claims it does not.
set -u
# Raw images are matched byte by byte, and bracket ranges must not follow a locale's collation.
export LC_ALL=C

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

# The interpreter read-back and the core-library read-back live beside this script; both report
# through refuse and the status it sets here.
here=$(dirname "$0")
# shellcheck source=read_back_interpreter.sh
. "$here/read_back_interpreter.sh"
# shellcheck source=read_back_core.sh
. "$here/read_back_core.sh"

# Both routes build to a bare path on Linux/macOS and to Release\<name>.exe on Windows; a missing
# executable refuses by name rather than aborting, so the rest of its tree is still read.
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
else
    prove_symbol_pattern
    prove_core_pattern
    read_core_library
fi

for tree in "$@"; do
    if ! test -d "$tree"; then
        refuse "$tree is not a directory, so the consumer was never built there"
        continue
    fi
    for probe_name in integration_test universal_robots_probe kr6_probe; do
        if ! probe=$(locate_probe "$tree" "$probe_name"); then
            refuse "$tree built no consumer named $probe_name to read back"
            continue
        fi
        read_symbols "$probe"
        read_linkage "$probe"
    done
done

# meios's own subdirectory only: a fetched dependency that installs itself puts its headers beside
# meios's, and a match there says nothing about what meios exposes.
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
