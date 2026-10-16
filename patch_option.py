# -*- coding: utf-8 -*-
"""Adds the optional L/R time difference for the spot track."""
import io, os, re, sys

SRC = r"D:\VST3 Programming\SpotAligner\SpotAligner"
log = []


def patch(path, pairs, enc="utf-8"):
    with io.open(path, "r", encoding=enc, newline="") as f:
        text = f.read()
    for old, new, label in pairs:
        if old in text:
            text = text.replace(old, new, 1)
            log.append("OK   %s :: %s" % (os.path.basename(path), label))
        else:
            log.append("MISS %s :: %s" % (os.path.basename(path), label))
    with io.open(path, "w", encoding=enc, newline="") as f:
        f.write(text)


# ---------------------------------------------------------------- parameters.h
p = os.path.join(SRC, "source", "parameters.h")
patch(p, [
    ("\tkParamSpotDistance,    ///< read only: how far the object is from the array, meters\n",
     "\tkParamSpotDistance,    ///< read only: how far the object is from the array, meters\n"
     "\tkParamDelayDifference, ///< whether the spot gets an L/R time difference (list)\n",
     "enum entry"),
    ("constexpr Steinberg::uint32 kStateVersion = 9;",
     "constexpr Steinberg::uint32 kStateVersionV9 = 9;\nconstexpr Steinberg::uint32 kStateVersion = 10;",
     "state version"),
    ("constexpr Steinberg::int32 kParamCountV8 = kParamMeterRightDb + 1;",
     "constexpr Steinberg::int32 kParamCountV8 = kParamMeterRightDb + 1;\n"
     "// Version 10 added the switch that decides whether the spot is given an L/R\n"
     "// time difference. Off, the two channels get one and the same delay, taken\n"
     "// from the microphone on the side the object is nearer - the traditional way.\n"
     "constexpr Steinberg::int32 kParamCountV9 = kParamSpotDistance + 1;",
     "count v9"),
    ("\tif (version == kStateVersionV8)\n\t\treturn kParamCountV8;",
     "\tif (version == kStateVersionV8)\n\t\treturn kParamCountV8;\n"
     "\tif (version == kStateVersionV9)\n\t\treturn kParamCountV9;",
     "count lookup"),
    ("\t    {0.0, kMaxDistanceMeters},                      // kParamSpotDistance (read only)",
     "\t    {0.0, kMaxDistanceMeters},                      // kParamSpotDistance (read only)\n"
     "\t    {0.0, 1.0},                                     // kParamDelayDifference (list: yes, no)",
     "range"),
    ("\t    0.0,                                                     // kParamSpotDistance (read only)",
     "\t    0.0,                                                     // kParamSpotDistance (read only)\n"
     "\t    1.0,                                                     // kParamDelayDifference (yes)",
     "default"),
])

# ------------------------------------------------------------ arrayParameters.cpp
p = os.path.join(SRC, "source", "arrayParameters.cpp")
patch(p, [
    ("\t// Saying that the array is no longer the technique it is named after.",
     "\t// Whether the spot is given a left/right time difference. On, the two\n"
     "\t// channels are delayed by the time the sound takes to reach their own\n"
     "\t// microphone, so the object is placed by the geometry. Off, both channels\n"
     "\t// get one and the same delay - the one of the microphone on the side the\n"
     "\t// object is nearer, and the left one when it sits on the centre line.\n"
     "\t{\n"
     "\t\tauto* difference = new StringListParameter (STR16 (\"L/R Time Difference\"),\n"
     "\t\t                                            kParamDelayDifference, nullptr,\n"
     "\t\t                                            ParameterInfo::kCanAutomate | ParameterInfo::kIsList);\n"
     "\t\tdifference->appendString (STR16 (\"Yes\"));\n"
     "\t\tdifference->appendString (STR16 (\"No\"));\n"
     "\t\tsetListDefault (*difference, 0, 2);\n"
     "\t\tcontainer.addParameter (difference);\n"
     "\t}\n\n"
     "\t// Saying that the array is no longer the technique it is named after.",
     "registration"),
])

# ------------------------------------------------------------------ spotPanner.h
p = os.path.join(SRC, "source", "spotPanner.h")
patch(p, [
    ("inline StereoSetup stereoSetup (const ArrayGeometry& array, const ObjectPlacement& object, double speedOfSound)",
     "inline StereoSetup stereoSetup (const ArrayGeometry& array, const ObjectPlacement& object, double speedOfSound,\n"
     "                                bool channelTimeDifference = true)",
     "signature"),
    ("\tsetup.right.delaySeconds = distanceToMicrophone (array, object, false) / speedOfSound;\n",
     "\tsetup.right.delaySeconds = distanceToMicrophone (array, object, false) / speedOfSound;\n\n"
     "\tif (!channelTimeDifference)\n"
     "\t{\n"
     "\t\t// The traditional way: one delay for the whole object, taken from the\n"
     "\t\t// microphone on the side it is nearer, and from the left one when it sits\n"
     "\t\t// on the centre line. Nothing then differs between the two channels, so\n"
     "\t\t// whatever time difference the object arrived with is left untouched - it\n"
     "\t\t// keeps the width it was recorded with, and the position is set by the\n"
     "\t\t// level difference alone.\n"
     "\t\tconst double left = distanceToMicrophone (array, object, true);\n"
     "\t\tconst double right = distanceToMicrophone (array, object, false);\n"
     "\t\tconst double chosen = (right < left) ? setup.right.delaySeconds : setup.left.delaySeconds;\n\n"
     "\t\tsetup.left.delaySeconds = chosen;\n"
     "\t\tsetup.right.delaySeconds = chosen;\n"
     "\t}\n",
     "single delay branch"),
])

# ----------------------------------------------------------------- processor.cpp
p = os.path.join(SRC, "source", "processor.cpp")
patch(p, [
    ("return Panorama::stereoSetup (currentArray (), currentObject (), usableSpeedOfSound (parameterValues));",
     "// The switch is a list: 0 = give the two channels their own delay, 1 = one\n"
     "\t// delay for the whole object (the traditional way).\n"
     "\tconst bool channelTimeDifference = parameterValues[kParamDelayDifference] < 0.5;\n\n"
     "\treturn Panorama::stereoSetup (currentArray (), currentObject (), usableSpeedOfSound (parameterValues),\n"
     "\t                              channelTimeDifference);",
     "pass the flag"),
])

# ------------------------------------------------------------------ editor.uidesc
p = os.path.join(SRC, "resource", "editor.uidesc")
with io.open(p, "r", encoding="utf-8", newline="") as f:
    xml = f.read()

# a control tag, copied from the pattern tag line
m = re.search(r'(\s*)<control-tag name="SpotPattern" tag="13"/>', xml)
if m:
    xml = xml.replace(m.group(0), m.group(0) + m.group(1) + '<control-tag name="DelayDifference" tag="31"/>', 1)
    log.append("OK   editor.uidesc :: control tag")
else:
    log.append("MISS editor.uidesc :: control tag")

# the row itself: a label and a checkbox, the checkbox copied from the bypass one
m = re.search(r'([ \t]*)<view class="CCheckBox"([^>]*?)control-tag="Bypass"([^>]*?)/>', xml)
if m:
    indent = m.group(1)
    box = (indent + '<view class="CCheckBox" control-tag="DelayDifference" origin="200, 322" size="18, 18"'
           ' transparent="true"/>')
    label = (indent + '<view class="CTextLabel" font="Label" font-color="Text" origin="16, 324" size="180, 18"'
             ' title="L/R time difference" transparent="true"/>')
    anchor = re.search(r'([ \t]*)<view class="PanoramaView"', xml)
    if anchor:
        xml = xml.replace(anchor.group(0), label + "\n" + box + "\n" + anchor.group(0), 1)
        log.append("OK   editor.uidesc :: row")
    else:
        log.append("MISS editor.uidesc :: panorama anchor")
else:
    log.append("MISS editor.uidesc :: checkbox markup")

# the window grows by 26 px, and both size attributes have to agree
before = xml
xml = xml.replace('EditorSize="0, 0, 688, 336"', 'EditorSize="0, 0, 688, 362"')
xml = re.sub(r'(<template[^>]*?)size="688, 336"', r'\1size="688, 362"', xml)
log.append("OK   editor.uidesc :: window height" if xml != before else "MISS editor.uidesc :: window height")

with io.open(p, "w", encoding="utf-8", newline="") as f:
    f.write(xml)

print("\n".join(log))
