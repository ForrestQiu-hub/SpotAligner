# -*- coding: utf-8 -*-
"""Removes the leftover diagnostic shells by their shape, not by line number."""
import io, os

SRC = r"D:\VST3 Programming\SpotAligner\SpotAligner\source"
FILES = ["speedSourceView.cpp", "formatOptionsView.cpp", "panoramaView.cpp", "processor.cpp"]
MARK = "// removed: kept the plug-in from writing diagnostics"
SHELL = ("void", "voidconst", "std::string")
report = []

for filename in FILES:
    path = os.path.join(SRC, filename)
    with io.open(path, "r", encoding="utf-8", newline="") as f:
        text = f.read()
    lines = text.split("\n")
    out = []
    i = 0
    removed = 0
    while i < len(lines):
        stripped = lines[i].strip()
        if stripped.startswith(MARK):
            # this is the body of a shell: drop it, the header above and the brace below
            while out and out[-1].strip() == "":
                out.pop()
            if out and out[-1].strip().rstrip("{").strip() in SHELL:
                out.pop()
            # and the doc comment that introduced it
            while out and out[-1].strip().startswith("*/") is False and (
                    out[-1].strip().startswith("//") or out[-1].strip().startswith("/**")
                    or out[-1].strip().startswith("*") or out[-1].strip() == ""):
                out.pop()
            if out and out[-1].strip().endswith("*/"):
                out.pop()
            while out and out[-1].strip().startswith("//-"):
                out.pop()
            i += 1                                     # skip the mark
            while i < len(lines) and lines[i].strip() != "}":
                i += 1
            i += 1                                     # skip the closing brace
            removed += 1
            continue
        out.append(lines[i])
        i += 1
    # the stray line the earlier repair appended at the very end
    while out and out[-1].strip() == "":
        out.pop()
    stray = 0
    if out and out[-1].strip() == "return moved;":
        out.pop()
        stray = 1
        while out and out[-1].strip() == "":
            out.pop()
    with io.open(path, "w", encoding="utf-8", newline="") as f:
        f.write("\n".join(out) + "\n")
    report.append("%-22s shells removed: %d   stray line removed: %d" % (filename, removed, stray))

print("\n".join(report))
