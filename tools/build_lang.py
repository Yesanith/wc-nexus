#!/usr/bin/env python3
"""Turns the translation tables (src/core/lang/<code>.json, each English text with what it says in that language) into
src/core/lang/Tables.inc, the C++ the addon is built with. Run it after changing a table; the check of the build fails when the file is not
the one the tables make.

    python3 tools/build_lang.py           writes the file
    python3 tools/build_lang.py --check   says whether the file is up to date
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
LANG = os.path.normpath(os.path.join(HERE, "..", "src", "core", "lang"))
CODES = ("de", "es", "fr", "pt", "tr", "ru", "ja", "zh")


def literal(s):
    out = []
    for ch in s:
        if ch == "\\":
            out.append("\\\\")
        elif ch == '"':
            out.append('\\"')
        elif ch == "\n":
            out.append("\\n")
        elif ch == "\t":
            out.append("\\t")
        else:
            out.append(ch)
    return 'u8"' + "".join(out) + '"'


def render():
    lines = ["// made by tools/build_lang.py from the json tables next to it. do not change it by hand", ""]
    for code in CODES:
        with open(os.path.join(LANG, code + ".json"), encoding="utf-8") as f:
            table = json.load(f)
        lines.append("static const Pair kTable_%s[] = {" % code)
        for k in sorted(table):
            lines.append("    {%s, %s}," % (literal(k), literal(table[k])))
        lines.append("};")
        lines.append("")
    lines.append("static const Table kTables[] = {")
    for code in CODES:
        lines.append('    {"%s", kTable_%s, sizeof(kTable_%s) / sizeof(Pair)},' % (code, code, code))
    lines.append("};")
    return "\n".join(lines) + "\n"


def main():
    target = os.path.join(LANG, "Tables.inc")
    text = render()
    if "--check" in sys.argv:
        current = open(target, encoding="utf-8").read() if os.path.exists(target) else ""
        if current != text:
            print("src/core/lang/Tables.inc is not the one the tables make: run python3 tools/build_lang.py")
            return 1
        print("the tables are up to date")
        return 0
    with open(target, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    print("wrote", target)
    return 0


if __name__ == "__main__":
    sys.exit(main())
