# Sourced by read_back_consumer.sh: the core library must carry no edge to the auxiliary-format
# module or its third-party dependency, which on an archive is an undefined reference left for the
# consumer's linker to satisfy. Only the nm legs read it; a Windows import library has no reader
# here that a control could prove, so that leg is not claimed.
core_pattern='(^|[^A-Za-z0-9_])YAML::|make_yaml_parser'

# nm succeeds on a stripped archive and prints nothing, so emptiness is read as well as the status.
count_core_edges()
{
    undefined=$(nm -C -u "$1" 2>/dev/null) && [ -n "$undefined" ] || return 1
    printf '%s\n' "$undefined" | grep -cE "$core_pattern" || true
}

# A compiled object that calls into both names must read as carrying them before a zero is trusted.
prove_core_pattern()
{
    scratch=$(mktemp -d) || { refuse "no scratch directory was available for the core control"; return; }
    printf 'namespace YAML { void Load(); }\nnamespace meios { void make_yaml_parser(); }\nvoid use() { YAML::Load(); meios::make_yaml_parser(); }\n' \
        > "$scratch/control.cpp"
    if ! c++ -c "$scratch/control.cpp" -o "$scratch/control.o" 2>/dev/null; then
        refuse "the core control could not be compiled, so the core read-back is unproven"
    elif ! seen=$(count_core_edges "$scratch/control.o") || [ "$seen" -ne 2 ]; then
        refuse "a compiled control referencing YAML:: and make_yaml_parser did not read as both, so the core read-back is blind"
    fi
    rm -rf "$scratch"
}

read_core_library()
{
    core=""
    for candidate in "$install"/lib*/libmeios_core.a "$install"/lib*/libmeios_core.so "$install"/lib*/libmeios_core.dylib; do
        test -f "$candidate" && core="$candidate" && break
    done
    if [ -z "$core" ]; then
        refuse "$install staged no meios_core library to read back"
    elif ! edges=$(count_core_edges "$core"); then
        refuse "$core yielded no undefined-symbol table, so nothing was read back from it"
    elif [ "$edges" -ne 0 ]; then
        refuse "$core references the auxiliary-format module or its dependency"
    fi
}
