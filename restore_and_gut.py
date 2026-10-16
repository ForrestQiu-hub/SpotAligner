# -*- coding: utf-8 -*-
"""Re-applies the switch and empties the diagnostic bodies, safely."""
import io, os, re

SRC = r"D:\VST3 Programming\SpotAligner\SpotAligner\source"
log = []

# ---------------------------------------------------------------- the switch
path = os.path.join(SRC, "processor.cpp")
with io.open(path, "r", encoding="utf-8", newline="") as f:
    text = f.read()
m = re.search(r"[ \t]*return Panorama::stereoSetup \(currentArray \(\), currentObject \(\), "
              r"usableSpeedOfSound \(parameterValues\)\);\r?\n", text)
if m and "kParamDelayDifference" not in text:
    nl = "\r\n" if "\r\n" in text else "\n"
    new = ("\t// The switch is a list: 0 = Off, one delay for the whole object (the traditional" + nl +
           "\t// way); 1 = On, the two channels are delayed to their own microphone." + nl +
           "\tconst bool channelTimeDifference = parameterValues[kParamDelayDifference] > 0.5;" + nl +
           nl +
           "\treturn Panorama::stereoSetup (currentArray (), currentObject (), usableSpeedOfSound (parameterValues)," + nl +
           "\t                              channelTimeDifference);" + nl)
    text = text[:m.start()] + new + text[m.end():]
    log.append("OK   processor.cpp :: switch re-applied")
else:
    log.append("MISS processor.cpp :: switch" + (" (already there)" if "kParamDelayDifference" in text else ""))
with io.open(path, "w", encoding="utf-8", newline="") as f:
    f.write(text)


# ---------------------------------------------------------------- the bodies
def gut(text, name, replacement):
    m = re.search(r"\b(?:void|bool|std::string)\s+" + re.escape(name) + r"\s*\([^;{]*\)[^{;]*\{", text)
    if not m:
        return text, False
    start = m.end() - 1
    depth = 0
    for i in range(start, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return text[:start + 1] + replacement + text[i:], True
    return text, False


TARGETS = {
    "processor.cpp": [("writeBusDiagnostics", "\n\t\t// no file logging in a released build\n\t"),
                      ("forceMonoTrackForTesting", "\n\t// no test hooks in a released build\n\treturn false;\n")],
    "formatOptionsView.cpp": [("writeDiagnostics", "\n\t\t// no file logging in a released build\n\t")],
    "speedSourceView.cpp": [("writeDiagnostics", "\n\t\t// no file logging in a released build\n\t")],
    "panoramaView.cpp": [("writeFollowDiagnostics", "\n\t\t// no file logging in a released build\n\t"),
                         ("writeDragDiagnostics", "\n\t\t// no file logging in a released build\n\t")],
}
for filename, items in TARGETS.items():
    path = os.path.join(SRC, filename)
    with io.open(path, "r", encoding="utf-8", newline="") as f:
        text = f.read()
    for name, replacement in items:
        text, ok = gut(text, name, replacement)
        log.append(("OK   " if ok else "MISS ") + filename + " :: gut " + name)
    with io.open(path, "w", encoding="utf-8", newline="") as f:
        f.write(text)

print("\n".join(log))
