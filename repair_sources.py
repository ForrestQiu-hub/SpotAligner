# -*- coding: utf-8 -*-
"""Repairs the four files: removes the leftover diagnostic shells and restores
the one statement that lost its body."""
import io, os

SRC = r"D:\VST3 Programming\SpotAligner\SpotAligner\source"

# every range is inclusive and refers to the file as it stands right now;
# they are applied from the bottom up so the earlier numbers stay valid
JOBS = [
    ("processor.cpp", [(58, 63), (34, 56)]),
    ("speedSourceView.cpp", [(115, 120)]),
    ("formatOptionsView.cpp", [(87, 92)]),
    ("panoramaView.cpp", [(459, 465), (408, 413), (365, 366)]),
]
REPLACE = {
    ("panoramaView.cpp", 365): ["\t\treturn moved;"],
}

for filename, ranges in JOBS:
    path = os.path.join(SRC, filename)
    with io.open(path, "r", encoding="utf-8", newline="") as f:
        lines = f.read().split("\r\n")
    for start, end in sorted(ranges, reverse=True):
        replacement = REPLACE.get((filename, start))
        if replacement is None:
            # swallow a following blank line as well, if there is one
            cut_end = end
            if cut_end < len(lines) and lines[cut_end].strip() == "":
                cut_end += 1
            del lines[start - 1:cut_end]
            print("  %-22s removed lines %d..%d" % (filename, start, end))
        else:
            lines[start - 1:end] = replacement
            print("  %-22s restored lines %d..%d -> %s" % (filename, start, end, replacement[0].strip()))
    with io.open(path, "w", encoding="utf-8", newline="") as f:
        f.write("\r\n".join(lines))
print("done")
