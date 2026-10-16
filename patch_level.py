# -*- coding: utf-8 -*-
"""With the time difference off, express the placement as level."""
import io, os

P = r"D:\VST3 Programming\SpotAligner\SpotAligner\source\spotPanner.h"
log = []

with io.open(P, "r", encoding="utf-8", newline="") as f:
    t = f.read()


def sub(old, new, label):
    global t
    if old in t:
        t = t.replace(old, new, 1)
        log.append("OK   " + label)
    else:
        log.append("MISS " + label)


# 1. the converted amount has to survive until the gains are known
sub("\tStereoSetup setup;\n",
    "\tStereoSetup setup;\n"
    "\t// How much of the placement the time difference carries, converted to the\n"
    "\t// level difference that means the same thing. Used when the time\n"
    "\t// difference is switched off.\n"
    "\tdouble extraLevelDb = 0.0;\n",
    "declare the amount")

# 2. read the time difference before it is levelled out
sub("""		setup.left.delaySeconds = chosen;
		setup.right.delaySeconds = chosen;
	}
""",
    """		// One full shift of the image is 18 dB of level or 1.5 ms of time, so
		// this is what the time difference was worth before it is taken away.
		extraLevelDb = (setup.right.delaySeconds - setup.left.delaySeconds) * 1000.0 / 1.5 * 18.0;

		setup.left.delaySeconds = chosen;
		setup.right.delaySeconds = chosen;
	}
""",
    "convert the time difference")

# 3. add it to the level difference the geometry gives, before the centre
#    microphone and the power normalisation are worked out
sub("""	setup.right.gain = polarResponse (array.patternA, offAxisDegrees (array, object, false)) /
	                   (rightDistance > 1.0e-6 ? rightDistance : 1.0e-6);
""",
    """	setup.right.gain = polarResponse (array.patternA, offAxisDegrees (array, object, false)) /
	                   (rightDistance > 1.0e-6 ? rightDistance : 1.0e-6);

	// With the time difference switched off the placement has to travel as
	// level, or the object would hardly move at all: a wide pair at a working
	// distance differs by a fraction of a dB between its two paths, while the
	// image it makes comes from its time difference. The conversion above is
	// added to that fraction, which is what makes the traditional single delay
	// still place the object where the geometry says it is. A coincident array
	// has no time difference to convert and is left exactly as it was.
	if (extraLevelDb != 0.0)
	{
		const double leftGain = setup.left.gain > 1.0e-9 ? setup.left.gain : 1.0e-9;
		const double rightGain = setup.right.gain > 1.0e-9 ? setup.right.gain : 1.0e-9;
		const double wantedDb = 20.0 * std::log10 (leftGain / rightGain) + extraLevelDb;

		// keep the pair's loudness: only the ratio between the sides changes
		const double ratio = std::pow (10.0, wantedDb / 20.0);
		const double powerIn = setup.left.gain * setup.left.gain + setup.right.gain * setup.right.gain;
		const double scale = std::sqrt (powerIn / (ratio * ratio + 1.0));

		setup.left.gain = scale * ratio;
		setup.right.gain = scale;
	}
""",
    "apply the level")

with io.open(P, "w", encoding="utf-8", newline="") as f:
    f.write(t)

print("\n".join(log))
