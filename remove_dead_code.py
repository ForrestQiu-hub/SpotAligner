# -*- coding: utf-8 -*-
"""Removes the emptied diagnostic functions and every call to them."""
import io, os, re

SRC = r"D:\VST3 Programming\SpotAligner\SpotAligner\source"
log = []

FUNCS = {
    "formatOptionsView.cpp": ["writeDiagnostics"],
    "panoramaView.cpp": ["writeFollowDiagnostics", "writeDragDiagnostics"],
    "speedSourceView.cpp": ["writeDiagnostics"],
    "processor.cpp": ["writeBusDiagnostics", "hostNameFrom"],
}


def cut_function(text, name):
    """Remove the definition, the matching braces, and the doc comment above it."""
    m = re.search(r"\bvoid\s+" + re.escape(name) + r"\s*\(", text)
    if not m:
        m = re.search(r"\bstd::string\s+" + re.escape(name) + r"\s*\(", text)
    if not m:
        return text, False
    brace = text.index("{", m.end())
    depth = 0
    for i in range(brace, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                start = m.start()
                # take the doc comment along
                head = text.rfind("/**", 0, start)
                if head != -1 and text[head:start].strip().endswith("*/"):
                    start = head
                end = i + 1
                while end < len(text) and text[end] in "\r\n":
                    end += 1
                while start > 0 and text[start - 1] in " \t":
                    start -= 1
                if start > 0 and text[start - 1] == "\n":
                    start -= 1
                if start > 0 and text[start - 1] == "\r":
                    start -= 1
                return text[:start] + text[end:], True
    return text, False


def cut_call(text, name):
    """Remove every statement that calls name, arguments and all."""
    removed = 0
    while True:
        m = re.search(r"(?<![\w.])" + re.escape(name) + r"\s*\(", text)
        if not m:
            break
        depth = 0
        i = m.end() - 1
        while i < len(text):
            if text[i] == "(":
                depth += 1
            elif text[i] == ")":
                depth -= 1
                if depth == 0:
                    break
            i += 1
        j = i + 1
        while j < len(text) and text[j] in " \t":
            j += 1
        if j < len(text) and text[j] == ";":
            j += 1
        # and the whitespace in front of the statement
        start = m.start()
        while start > 0 and text[start - 1] in " \t":
            start -= 1
        line_start = text.rfind("\n", 0, start) + 1
        if text[line_start:start].strip() == "":
            start = line_start
        while j < len(text) and text[j] in "\r\n":
            j += 1
        text = text[:start] + text[j:]
        removed += 1
    return text, removed


for filename, names in FUNCS.items():
    path = os.path.join(SRC, filename)
    with io.open(path, "r", encoding="utf-8", newline="") as f:
        text = f.read()
    for name in names:
        text, n = cut_call(text, name)
        text, ok = cut_function(text, name)
        log.append("%-24s %-24s calls removed: %d  definition removed: %s"
                   % (filename, name, n, ok))
    with io.open(path, "w", encoding="utf-8", newline="") as f:
        f.write(text)

print("\n".join(log))
