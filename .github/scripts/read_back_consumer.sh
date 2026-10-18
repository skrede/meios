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

# A count taken straight from a pipeline reads zero when its producer could not run, which is also
# the clean result, so each producer's exit status is read before its output is counted. The Windows
# byte scan has no producer to fail; the self-proof and the image check below stand in for one.
dll_pattern='python3[0-9]{0,3}\.dll|python[0-9]\.[0-9]+\.dll'
# PEP 7 (https://peps.python.org/pep-0007/): public C names carry a Py, Py_ or Py<Group>_ prefix and
# internal ones _Py; Mach-O prepends one more underscore, so up to three leading ones are allowed.
api_pattern='_{0,3}Py[A-Z_][A-Za-z0-9_]{2,}'

# An import table names each function as a NUL-terminated string, so only an identifier ending at a
# NUL is read as a name; a prefix match anywhere in code bytes would be chance.
count_api_names()
{
    tr '\0' '\n' < "$1" | grep -aoE '[A-Za-z0-9_]+$' | grep -cE "^${api_pattern}$"
}

# Emptiness is read as well as the exit status: nm succeeds on a stripped image and prints nothing,
# which counts the same as a clean subject while having read nothing at all.
count_symbols()
{
    symbols=$(nm -C "$1" 2>/dev/null) && [ -n "$symbols" ] || return 1
    printf '%s\n' "$symbols" | grep -cE "(^|[^A-Za-z0-9_])${api_pattern}|pybind11" || true
}

count_bindings()
{
    grep -aoE 'pybind11' "$1" 2>/dev/null | wc -l
}

count_dll_imports()
{
    grep -aoiE "$dll_pattern" "$1" 2>/dev/null | wc -l
}

# A negative check that cannot fail proves nothing: both patterns must first match the running
# interpreter's own executable, which imports Py_Main from python3NN.dll. A missing reference
# refuses rather than skips, so the leg cannot quietly fall back to an unproven check.
prove_interpreter_pattern()
{
    reference=$(command -v python 2>/dev/null || command -v python3 2>/dev/null)
    # Under MSYS command -v names an .exe without its suffix, and reading the image needs the file.
    if [ -n "$reference" ] && [ ! -f "$reference" ] && [ -f "$reference.exe" ]; then
        reference="$reference.exe"
    fi
    if [ -z "$reference" ] || [ ! -f "$reference" ]; then
        refuse "no interpreter executable was found to prove the read-back pattern against"
        return
    fi
    seen=$(count_dll_imports "$reference")
    test "$seen" -gt 0 \
        || refuse "the interpreter-DLL pattern matched nothing in $reference, so this read-back is blind"
    seen=$(count_api_names "$reference")
    test "$seen" -gt 0 \
        || refuse "the interpreter-symbol pattern matched nothing in $reference, so this read-back is blind"
}

# The nm legs' counterpart of the proof above: a program compiled here, defining the interpreter's
# entry point, must read as carrying it before a zero from the same reader is trusted.
prove_symbol_pattern()
{
    scratch=$(mktemp -d) || { refuse "no scratch directory was available for the symbol control"; return; }
    printf 'extern "C" int Py_Main(int, wchar_t**) { return 0; }\nint main() { return Py_Main(0, 0); }\n' \
        > "$scratch/control.cpp"
    if ! c++ "$scratch/control.cpp" -o "$scratch/control" 2>/dev/null; then
        refuse "the symbol control could not be compiled, so this read-back is unproven"
    elif ! seen=$(count_symbols "$scratch/control") || [ "$seen" -eq 0 ]; then
        refuse "a compiled control carrying Py_Main read as clean, so this read-back is blind"
    fi
    rm -rf "$scratch"
}

# A byte scan of an empty or unreadable file finds no name at all, which is the clean result, so a
# subject must be readable and open with the DOS stub's MZ signature before its zero counts.
is_pe_image()
{
    test -r "$1" && test "$(head -c 2 "$1" 2>/dev/null)" = "MZ"
}

read_symbols()
{
    probe="$1"
    if [ "$plat" = "windows" ]; then
        if ! is_pe_image "$probe"; then
            refuse "$probe is not a readable PE image, so nothing was read back from it"
            return
        fi
        carried=$(( $(count_api_names "$probe") + $(count_bindings "$probe") ))
        test "$carried" -eq 0 || refuse "$probe carries an interpreter symbol"
        return
    fi
    if carried=$(count_symbols "$probe"); then
        test "$carried" -eq 0 || refuse "$probe carries an interpreter symbol"
    else
        refuse "$probe yielded no symbol table, so nothing was read back from it"
    fi
}

read_linkage()
{
    probe="$1"
    if [ "$plat" = "windows" ]; then
        linked=$(count_dll_imports "$probe")
        test "$linked" -eq 0 || refuse "$probe carries an interpreter DLL import"
        return
    fi
    lister=(ldd)
    [ "$plat" = "darwin" ] && lister=(otool -L)
    if linkage=$("${lister[@]}" "$probe" 2>/dev/null); then
        linked=$(printf '%s\n' "$linkage" | grep -ci python || true)
        test "$linked" -eq 0 || refuse "$probe links an interpreter shared object"
    else
        refuse "$probe yielded no dynamic linkage, so nothing was read back from it"
    fi
}

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
