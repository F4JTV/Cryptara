#!/usr/bin/env bash
# The Windows files must carry CRLF endings: cmd.exe misparses a batch file
# with bare line feeds ("X was unexpected at this time"), and the .iss must
# keep its UTF-8 byte-order mark or Inno Setup garbles the French texts.
# Fails when one of them is wrong; repairs the endings with --fix.
set -u
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
status=0
for f in "$SRC_DIR"/build_all.bat "$SRC_DIR"/installer/*.iss "$SRC_DIR"/assets/*.rc; do
    [ -f "$f" ] || continue
    total=$(awk 'END{print NR}' "$f")
    crlf=$(awk 'BEGIN{n=0} /\r$/ {n++} END{print n}' "$f")
    if [ "$crlf" -ne "$total" ]; then
        if [ "${1:-}" = "--fix" ]; then
            sed -i 's/\r$//; s/$/\r/' "$f"
            echo "fixed: $f"
        else
            echo "not CRLF throughout ($crlf of $total lines): $f"
            status=1
        fi
    fi
done
for f in "$SRC_DIR"/installer/*.iss; do
    [ -f "$f" ] || continue
    if [ "$(head -c 3 "$f" | od -An -tx1 | tr -d ' ')" != "efbbbf" ]; then
        echo "no UTF-8 byte-order mark: $f"
        status=1
    fi
done
[ $status -eq 0 ] && echo "Windows line endings and byte-order mark in order."
exit $status
