# -*- coding: utf-8 -*-
"""Empties every temporary diagnostic that writes into %TEMP%."""
import io, os, re

SRC = r"D:\VST3 Programming\SpotAligner\SpotAligner\source"
TARGETS = {
    "controller.cpp": ["writeReadoutDiagnostics", "updateComputedReadouts"],
    "formatOptionsView.cpp": ["writeDiagnostics"],
    "panoramaView.cpp": ["writeFollowDiagnostics", "writeDragDiagnostics"],
    "processor.cpp": ["writeBusDiagnostics"],
    "speedSourceView.cpp": ["writeDiagnostics"],
}
log = []


def gut(text, name):
    """Replace the body of the first function called `name` with a comment."""
    m = re.search(r"\bvoid\s+" + re.escape(name) + r"\s*\([^;{]*\)[^{;]*\{", text)
    if not m:
        return text, False
    start = m.end() - 1                     # the opening brace
    depth = 0
    for i in range(start, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                body = "\n\t\t// removed: kept the plug-in from writing diagnostics into %TEMP%\n\t"
                return text[:start + 1] + body + text[i:], True
    return text, False


for filename, names in TARGETS.items():
    path = os.path.join(SRC, filename)
    with io.open(path, "r", encoding="utf-8", newline="") as f:
        text = f.read()
    for name in names:
        text, ok = gut(text, name)
        log.append(("OK   " if ok else "MISS ") + filename + " :: " + name)
    with io.open(path, "w", encoding="utf-8", newline="") as f:
        f.write(text)

# the delay diagnostic in the controller is a block, not a function: drop it whole
path = os.path.join(SRC, "controller.cpp")
with io.open(path, "r", encoding="utf-8", newline="") as f:
    text = f.read()
m = re.search(r"\n\t// Temporary: what the readouts were built from.*?\n\t\}\n", text, re.S)
if m:
    text = text[:m.start()] + "\n" + text[m.end():]
    log.append("OK   controller.cpp :: delay diagnostic block")
else:
    log.append("MISS controller.cpp :: delay diagnostic block")
with io.open(path, "w", encoding="utf-8", newline="") as f:
    f.write(text)

print("\n".join(log))
