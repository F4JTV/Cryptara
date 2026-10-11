#!/usr/bin/env python3
"""Static checks of build_all.bat against cmd.exe's parsing rules.

Three things leave only "X was unexpected at this time" behind when they go
wrong, so they are checked before the script ever reaches Windows:
  - inside a parenthesised block, an unquoted ')' in the text of an echo or
    a rem closes the block (a '(' in such text does NOT open one);
  - '/?' as the first word after rem or echo asks the command for its help
    instead of being text;
  - a batch file without CRLF line endings is misparsed.

    python3 scripts/check_batch.py [build_all.bat]
"""
import re
import sys
from pathlib import Path


def strip_quotes(line):
    """The line with quoted text blanked out, and ^-escaped parentheses removed."""
    out, quoted = [], False
    for ch in line:
        if ch == '"':
            quoted = not quoted
            out.append(ch)
        elif quoted:
            out.append(" ")
        else:
            out.append(ch)
    return "".join(out).replace("^)", "  ").replace("^(", "  ")


def check(path):
    data = path.read_bytes()
    problems = []
    if b"\r\n" not in data:
        problems.append((0, "no CRLF line endings"))
    elif data.replace(b"\r\n", b"").count(b"\n"):
        problems.append((0, "mixed line endings (bare LF)"))

    depth = 0
    for number, raw in enumerate(data.decode("utf-8", "replace").split("\r\n"), 1):
        line = raw.strip()
        if not line or line.startswith("::"):
            continue
        if re.match(r"(?i)^(rem|echo)\s+/\?(\s|$)", line):
            problems.append((number, "'/?' as the first word of rem/echo: " + line))
        text = strip_quotes(line)
        # A line that closes a block, possibly reopening one: ") else ("
        if text.startswith(")"):
            depth = max(0, depth - 1)
            text = text[1:].strip()
            if re.match(r"(?i)^else\s*\($", text):
                depth += 1
            continue
        if re.match(r"(?i)^(@?echo|rem)\b", text):
            if depth > 0 and ")" in text:
                problems.append((number, "')' in echo/rem text closes the enclosing block: " + line))
            continue
        # if / for / else / a lone '(' that opens a block on this line.
        if text.endswith("(") and (re.match(r"(?i)^(if|for|else)\b", text) or text == "("
                                   or re.search(r"(?i)\bdo\s*\($", text)):
            depth += 1
            continue
        opened = text.count("(")
        closed = text.count(")")
        if re.match(r"(?i)^(if|for)\b", text) and opened > closed:
            depth += opened - closed
    if depth != 0:
        problems.append((0, "unbalanced blocks (depth %d at the end)" % depth))
    return problems


def main():
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parent.parent / "build_all.bat"
    problems = check(path)
    for number, message in problems:
        print("%s:%s: %s" % (path.name, number or "-", message))
    if not problems:
        print("%s: cmd.exe parsing rules respected." % path.name)
    sys.exit(1 if problems else 0)


if __name__ == "__main__":
    main()
