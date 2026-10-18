#!/usr/bin/env sh
# usage: check_ci_table.sh [repository-root]
#
# Refuses when a platform marked in the README continuous-integration table names a job that the
# platform's workflow does not declare, or when the table itself can no longer be found.

root=${1:-.}
readme="$root/README.md"
header='| What is proven | Job | Linux | macOS | Windows |'

[ -f "$readme" ] || { echo "No README.md under $root"; exit 1; }

jobs_of()
{
    awk '/^jobs:/ { inside = 1; next }
         inside && /^[^ #]/ { inside = 0 }
         inside && /^  [A-Za-z0-9_-]+:[ \t]*$/ { sub(/^  /, ""); sub(/:.*/, ""); print }' \
        "$root/.github/workflows/$1.yml"
}

claims=$(awk -v header="$header" '
    $0 == header { found = 1; skip = 1; next }
    found && skip { skip = 0; next }
    found && !/^\|/ { exit }
    found {
        if (split($0, cell, "|") != 7) { print "malformed " NR; next }
        job = cell[3]
        if (!sub(/^[^`]*`/, "", job) || !sub(/`.*/, "", job)) { print "nojob " NR; next }
        if (cell[4] ~ /●/) print job " linux"
        if (cell[5] ~ /●/) print job " macos"
        if (cell[6] ~ /●/) print job " windows"
        rows++
    }
    END { if (!rows) print "empty 0" }' "$readme")

linux=$(jobs_of linux)
macos=$(jobs_of macos)
windows=$(jobs_of windows)

status=0
checked=0
while read -r job platform; do
    case $job in
    empty) echo "No rows found under the table header: $header"; exit 1 ;;
    malformed) echo "README.md:$platform: table row does not have five cells"; status=1; continue ;;
    nojob) echo "README.md:$platform: table row names no backtick-quoted job"; status=1; continue ;;
    esac
    eval "declared=\$$platform"
    if ! printf '%s\n' "$declared" | grep -qx -- "$job"; then
        echo "$job on $platform: marked in README.md but not a job in $platform.yml"
        status=1
    fi
    checked=$((checked + 1))
done <<CLAIMS
$claims
CLAIMS

[ "$status" -eq 0 ] && echo "Checked $checked platform claims in README.md: every job exists."
exit "$status"
