#!/usr/bin/env bash
# bump_version.sh NEW_VERSION "change" ["change" ...]
#
# Sets the version in CMakeLists.txt (the only place it is written: the
# program, the .exe version information and the installer all take it from
# there) and adds a section to CHANGELOG.md.  Runs in Git Bash on Windows.
set -euo pipefail
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NEW="${1:-}"
shift || true
[[ "$NEW" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo "Usage: $0 X.Y.Z \"change\" [\"change\" ...]" >&2; exit 2; }
[ $# -gt 0 ] || { echo "Give at least one line for the changelog." >&2; exit 2; }

OLD="$(sed -n 's/^project(CRYPTARA VERSION \([0-9.]*\) .*/\1/p' "$SRC_DIR/CMakeLists.txt" | head -1)"
[ -n "$OLD" ] || { echo "Could not read the current version from CMakeLists.txt" >&2; exit 1; }
[ "$OLD" != "$NEW" ] || { echo "Already at $NEW" >&2; exit 1; }

DATE_ISO="$(date +%Y-%m-%d)"

# CMakeLists.txt
sed -i "s/^project(CRYPTARA VERSION $OLD /project(CRYPTARA VERSION $NEW /" "$SRC_DIR/CMakeLists.txt"

# CHANGELOG.md: a new section before the first one.
SECTION="$(printf '## %s - %s\n\n%s\n' "$NEW" "$DATE_ISO" "$(printf -- '- %s\n' "$@")")
"
export SECTION
awk 'BEGIN {done=0} /^## / && !done {print ENVIRON["SECTION"]; done=1} {print}' \
    "$SRC_DIR/CHANGELOG.md" > "$SRC_DIR/CHANGELOG.md.new" \
    && mv "$SRC_DIR/CHANGELOG.md.new" "$SRC_DIR/CHANGELOG.md"

echo "$OLD -> $NEW: CMakeLists.txt and CHANGELOG.md updated."
