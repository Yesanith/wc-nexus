#!/usr/bin/env python3
"""Reads the translation tables of the Blish HUD module (CodexEvents/Core/Lang/*.cs) into src/core/lang/<code>.json.

It was used once, to start the tables of this addon from those of the module: the old name of the app is replaced by the new one. The json files are
the tables from then on; tools/build_lang.py turns them into the C++ the addon is built with.

    python3 tools/import_blish_lang.py <path of codex-blish-module>
"""
import json
import os
import re
import sys

FILES = {"German": "de", "Spanish": "es", "French": "fr", "Portuguese": "pt", "Turkish": "tr", "Russian": "ru", "Japanese": "ja", "Chinese": "zh"}
STRING = r'"((?:[^"\\]|\\.)*)"'
ENTRY = re.compile(r'\{\s*' + STRING + r'\s*,\s*' + STRING + r'\s*\}')
RENAME = (("Tyria Codex Events", "Wondrous Compass"), ("Tyria-Codex-Events", "Wondrous-Compass"), ("Tyria Codex", "Wondrous Compass"),
          ("Tyria-Codex", "Wondrous-Compass"))


def unescape(s):
    return s.encode("utf-8").decode("unicode_escape").encode("latin-1").decode("utf-8") if "\\" in s else s


def renamed(s):
    for a, b in RENAME:
        s = s.replace(a, b)
    return s


def main(root):
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "core", "lang")
    os.makedirs(out, exist_ok=True)
    for name, code in FILES.items():
        text = open(os.path.join(root, "CodexEvents", "Core", "Lang", name + ".cs"), encoding="utf-8").read()
        table = {}
        for m in ENTRY.finditer(text):
            table[renamed(unescape(m.group(1)))] = renamed(unescape(m.group(2)))
        with open(os.path.join(out, code + ".json"), "w", encoding="utf-8") as f:
            json.dump(dict(sorted(table.items())), f, ensure_ascii=False, indent=1)
            f.write("\n")
        print(code, len(table))


if __name__ == "__main__":
    main(sys.argv[1])
