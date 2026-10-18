#!/usr/bin/env bash
# usage: read_back_controls.sh <read_back_consumer.sh> <case>
#
# Builds a scratch install prefix and consumer tree that differ from a clean pair in exactly the
# one way the case names, runs the read-back over them, and requires the verdict the case expects:
# a refusal must exit 1 and say why, and the clean pair must exit 0.
set -u
export LC_ALL=C

if [ "$#" -ne 2 ]; then
    echo "usage: $(basename "$0") <read_back_consumer.sh> <case>" >&2
    exit 2
fi
subject="$1"
case_name="$2"

work=$(mktemp -d) || { echo "no scratch directory was available"; exit 2; }
trap 'chmod -R u+rwx "$work" 2>/dev/null; rm -rf "$work"' EXIT

fail()
{
    echo "FAIL: $1"
    exit 1
}

compile()
{
    c++ "$@" 2>/dev/null || fail "the fixture could not be compiled: $*"
}

stage_prefix()
{
    prefix="$work/prefix"
    mkdir -p "$prefix/include/meios/detail" "$prefix/lib"
    printf '#include <string>\nnamespace meios { std::string name(); }\n' > "$prefix/include/meios/name.h"
    printf 'void edge();\nvoid use() { edge(); }\n' > "$work/core.cpp"
    compile -c "$work/core.cpp" -o "$work/core.o"
    ar rcs "$prefix/lib/libmeios_core.a" "$work/core.o" || fail "the core archive could not be staged"
}

stage_tree()
{
    tree="$work/tree"
    mkdir -p "$tree"
    printf 'int main() { return 0; }\n' > "$work/probe.cpp"
    compile "$work/probe.cpp" -o "$work/probe"
    for name in integration_test universal_robots_probe kr6_probe; do
        cp "$work/probe" "$tree/$name"
    done
}

# Each mutation spoils one thing in the clean pair and names the message that thing must raise.
mutate()
{
    case "$case_name" in
        clean)
            expected="" ;;
        header_names_dependency)
            printf '#include <yaml-cpp/yaml.h>\n' > "$prefix/include/meios/detail/leak.h"
            expected="an installed public header names the auxiliary-format module's own dependency" ;;
        header_names_namespace)
            printf 'namespace meios { YAML::Node node(); }\n' > "$prefix/include/meios/leak.h"
            expected="an installed public header names the auxiliary-format module's own dependency" ;;
        headers_unsearchable)
            [ "$(id -u)" -eq 0 ] && { echo "SKIP: an unreadable directory is readable to root"; exit 0; }
            chmod 000 "$prefix/include/meios/detail"
            expected="could not be searched, so no header was read back" ;;
        headers_missing)
            rm -rf "$prefix/include/meios"
            expected="is not a directory, so the install staged no public headers" ;;
        tree_missing)
            rm -rf "$tree"
            expected="is not a directory, so the consumer was never built there" ;;
        probe_missing)
            rm -f "$tree/kr6_probe"
            expected="built no consumer named kr6_probe to read back" ;;
        probe_not_an_object)
            printf 'not an executable image\n' > "$tree/kr6_probe"
            expected="yielded no symbol table, so nothing was read back from it" ;;
        core_missing)
            rm -f "$prefix/lib/libmeios_core.a"
            expected="staged no meios_core library to read back" ;;
        core_references_dependency)
            printf 'namespace YAML { void Load(); }\nvoid use() { YAML::Load(); }\n' > "$work/core.cpp"
            compile -c "$work/core.cpp" -o "$work/core.o"
            rm -f "$prefix/lib/libmeios_core.a"
            ar rcs "$prefix/lib/libmeios_core.a" "$work/core.o" || fail "the core archive could not be restaged"
            expected="references the auxiliary-format module or its dependency" ;;
        *)
            echo "unknown case $case_name" >&2
            exit 2 ;;
    esac
}

stage_prefix
stage_tree
mutate

output=$(bash "$subject" "$prefix" "$tree" 2>&1)
status=$?
printf '%s\n' "$output"

if [ -z "$expected" ]; then
    [ "$status" -eq 0 ] || fail "the clean pair was refused with status $status"
    [ -z "$output" ] || fail "the clean pair passed but reported something"
else
    [ "$status" -eq 1 ] || fail "the read-back exited $status where a refusal (1) was owed"
    printf '%s\n' "$output" | grep -qF "$expected" || fail "the refusal did not say: $expected"
fi
echo "PASS: $case_name"
