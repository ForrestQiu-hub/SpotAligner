//------------------------------------------------------------------------
// Copyright(c) 2026 Forrest's Plug-ins.
//------------------------------------------------------------------------
// The geometry a main microphone format stands for, and which part of it the
// user may change.
//
// Kept free of any VSTGUI or VST type on purpose, so that the rules can be
// checked by a headless test.
//
// The order of the entries is the order of the list parameter and must not
// change without a state version bump.
//
// This is what the DSP of the object is built from: the spacing gives the time
// difference between the two sides, the half angle and the polar pattern give
// the level difference. See spotPanner.h.
//
// Decca Tree is not in the list: it has a third, centre microphone and the
// plug-in produces two channels, so offering it would mean placing the object
// in an array whose middle microphone cannot be heard. The spacing it needs
// (up to 3 m) is still reachable through AB.
//------------------------------------------------------------------------
#pragma once

#include "spotPanner.h"

#include <cmath>

namespace MyCompanyName {
namespace Panorama {

//------------------------------------------------------------------------
/** The stereo techniques offered for the main microphone array.

	MS is not offered: its side signal is normally adjusted while mixing, so the
	pair the plug-in would model is not the pair that ends up in the recording.
	Its coincident equivalent is still covered by XY, whose published recording
	angle - 196 degrees for a cardioid pair at +-45 degrees - is what a purely
	level based pair of that width needs. */
enum MainFormat
{
	kFormatAB = 0,
	kFormatORTF,
	kFormatXY,
	kFormatNOS,
	kFormatDIN,
	kFormatDeccaTree,
	kFormatCount
};

/** Number of entries in the list parameter. */
constexpr int kMainFormatCount = kFormatCount;

/** Entry name of a format. */
inline const char* formatName (int format)
{
	switch (format)
	{
		case kFormatAB: return "AB";
		case kFormatORTF: return "ORTF";
		case kFormatXY: return "XY";
		case kFormatNOS: return "NOS";
		case kFormatDIN: return "DIN";
		case kFormatDeccaTree: return "Decca Tree";
		default: return "?";
	}
}

//------------------------------------------------------------------------
// The spacings the named techniques are defined with. They are part of the
// technique, so the UI shows them read only instead of letting them drift.
constexpr double kOrtfSpacingCm = 17.0;
constexpr double kNosSpacingCm = 30.0;
constexpr double kDinSpacingCm = 20.0;

/** The distance between the outer microphones a format is defined with, or 0
	when the format leaves it to the user (AB). */
inline double formatStandardSpacingCm (int format)
{
	switch (format)
	{
		case kFormatORTF: return kOrtfSpacingCm;
		case kFormatNOS: return kNosSpacingCm;
		case kFormatDIN: return kDinSpacingCm;
		default: return 0.0;
	}
}

/** True when the spacing comes from the technique and cannot be changed. */
inline bool formatLocksSpacing (int format)
{
	// A format fills the spacing in, and the value stays editable afterwards: a
	// pair spaced like an ORTF but aimed differently is a real technique.
	(void)format;
	return false;
}

/** The distance between the outer microphones is shown for a spaced pair and
	for the techniques that are defined with one. XY is coincident and
	have none. */
inline bool formatUsesSpacing (int format)
{
	// Only a coincident array has no distance to set. Everything else does, and
	// since a format is a preset rather than a straight jacket, the value is the
	// user's to change.
	return format != kFormatXY;
}

/** The spacing that is in use, in centimetres: the technique's own when it has
	one, otherwise the one the user set. This is what the DSP takes.

	A coincident format is zero whatever the spacing field happens to hold: the
	two microphones stand in the same place, so there is no distance between
	them to use, and taking the value left over from AB would give the array a
	time difference it cannot have. */
inline double formatSpacingCm (int format, double userSpacingCm)
{
	if (!formatUsesSpacing (format))
		return 0.0;

	const double standard = formatStandardSpacingCm (format);
	return standard > 0.0 ? standard : userSpacingCm;
}

/** No format in the list has a centre microphone any more. */
inline bool formatUsesCentreDistance (int format)
{
	return format == kFormatDeccaTree;
}

//------------------------------------------------------------------------
/** How far each of the two microphones is aimed away from the centre, in
	degrees. This is the angle that turns a position into a level difference:
	a source that sits where one microphone points is picked up by that one at
	full level and by the other one off its axis.

	AB is normally done with two parallel microphones, so both point forward. */
inline double formatHalfAngleDegrees (int format)
{
	switch (format)
	{
		case kFormatORTF: return 55.0;
		case kFormatNOS: return 45.0;
		case kFormatDIN: return 45.0;
		case kFormatDeccaTree: return 0.0; // a tree is normally omnis, pointing forward
		case kFormatXY: return 45.0;
		default: return 0.0; // AB: parallel
	}
}

/** Polar response of the array microphones, as the coefficient a of
	a + (1 - a) * cos(angle): 1 is omni, 0.5 cardioid, 0 figure of eight.

	AB is normally done with omnis, which is exactly why it produces a time
	difference and no level difference. The others are cardioids.

	The patterns of the other formats are all cardioid. */
inline double formatPatternA (int format)
{
	return (format == kFormatAB || format == kFormatDeccaTree) ? 1.0 : 0.5;
}

//------------------------------------------------------------------------
/** Stereo recording angle: the full angle of sound sources that the array maps
	into its stereo image.

	For a time difference array - two parallel omnis, which is what AB is - it
	follows from the path difference that produces a full phantom source shift,
	1.5 ms:

	    sin(SRA / 2) = speed of sound * 1.5 ms / spacing

	which reproduces the published table exactly: 60 cm gives 118 degrees,
	90 cm 68, 120 cm 50, and 51.5 cm the full 180. When the path difference
	cannot reach 1.5 ms anywhere in the front half, the array never reaches the
	extremes and every source stays near the middle, so the angle comes out
	larger than 180 - which is what a narrow array does.

	The aimed cardioid arrays take their published values instead, because
	mixing level and time differences needs curves that are not published in
	closed form.

	A Decca Tree has no published value either, and its centre microphone puts
	the answer out of reach of the formula above, because the centre stands in
	both channels and dilutes what the outer pair provides. Its angle is
	therefore found from the same cues the placement itself uses, by looking for
	the angle at which they add up to a full shift. There is no table to check
	that against; it is the model's own answer, and it says what the model does:
	adding a centre microphone makes the image narrower, so the same outer pair
	covers a wider angle.

	Source: Eberhard Sengpiel, UdK Berlin 1997, "Well-known stereo microphone
	systems and their angles". */
//------------------------------------------------------------------------
inline double formatPrescribedSpacingCm (int format)
{
	switch (format)
	{
		case kFormatORTF: return kOrtfSpacingCm;
		case kFormatNOS: return kNosSpacingCm;
		case kFormatDIN: return kDinSpacingCm;
		default: return 0.0;
	}
}
/** The spacing the named technique is defined with, if it prescribes one. Zero
	for the techniques that do not: a tree and a plain pair are spaced by the
	user, and a coincident pair has no spacing at all. */

/** Half of the published recording angle of a technique, in degrees. Only the
	techniques the table lists have one. */
inline double formatPublishedHalfAngle (int format)
{
	switch (format)
	{
		case kFormatORTF: return 48.0;  // 96
		case kFormatNOS: return 40.5;   // 81
		case kFormatDIN: return 50.5;   // 101
		case kFormatXY: return 98.0;    // 196
		default: return 0.0;
	}
}

/** True for the techniques whose recording angle is published in the table. */
inline bool formatHasPublishedAngle (int format)
{
	return format == kFormatORTF || format == kFormatNOS || format == kFormatDIN || format == kFormatXY;
}

/** True when the array is still exactly the technique its name stands for.
	Only then does the published recording angle describe it; as soon as one of
	the three values is changed the angle comes from the model instead. */
inline bool formatGeometryIsStandard (int format, const ArrayGeometry& array)
{
	if (!formatHasPublishedAngle (format))
		return false;

	if (std::fabs (array.spacingMeters * 100.0 - formatStandardSpacingCm (format)) > 0.01)
		return false;
	if (std::fabs (array.halfAngleDegrees - formatHalfAngleDegrees (format)) > 0.01)
		return false;

	return std::fabs (array.patternA - formatPatternA (format)) < 1.0e-6;
}

/** True when the technique is one that prescribes its own geometry, and the
	array is no longer it: the values were changed by hand. Techniques without a
	prescription - a plain pair, a tree - are whatever they are set to, so there
	is nothing to have deviated from. */
inline bool formatGeometryIsEdited (int format, const ArrayGeometry& array)
{
	return formatHasPublishedAngle (format) && !formatGeometryIsStandard (format, array);
}

inline double formatRecordingAngleDegrees (int format, const ArrayGeometry& array, double speedOfSound)
{
	// The published value describes the technique, so it is used exactly while
	// the array still is that technique.
	if (formatGeometryIsStandard (format, array))
	{
		switch (format)
		{
			case kFormatORTF: return 96.0;  // 17 cm, +-55 degrees
			case kFormatNOS: return 81.0;   // 30 cm, +-45 degrees
			case kFormatDIN: return 101.0;  // 20 cm, +-45 degrees
			case kFormatXY: return 196.0;   // coincident, +-45 degrees
			default: break;
		}
	}

	const double spacing = array.spacingMeters;

	// Two parallel omnis are the one case with an exact answer in closed form:
	// they differ in nothing but time, and only the path difference counts.
	if (array.patternA >= 1.0 - 1.0e-9 && array.centreDistanceMeters <= 0.0)
	{
		if (spacing <= 0.0 || speedOfSound <= 0.0)
			return 360.0;

		const double halfPath = speedOfSound * 0.0015;
		const double sine = halfPath / spacing;

		// exactly 1 is the full 180 degrees, and the comparison has to allow for
		// the rounding of a double; beyond it the array never reaches the
		// extremes anywhere in the front half, so everything stays near the
		// middle
		if (sine >= 1.0)
			return sine <= 1.0 + 1.0e-9 ? 180.0 : 360.0;

		const double pi = 3.14159265358979323846;
		return 2.0 * std::asin (sine) * 180.0 / pi;
	}

	// Everything else - an aimed pair, a centre microphone, or a pair whose
	// level difference is what does the work - is found by looking for the angle
	// at which the cues the placement itself uses add up to what they are at the
	// edge of the technique. A reference distance far enough away keeps the level
	// difference between the outer microphones out of it, so the angle stays a
	// property of the array and not of where the object happens to stand.
	{
		const double reference = 10.0;

		// The threshold is a full shift. For a technique with a published angle it
		// is the cue that technique has at its own edge, so changing one of its
		// values moves the angle away from the published one smoothly. Without
		// that the model answered 133 degrees for an ORTF whose table says 96, and
		// the readout jumped the moment the spacing was touched.
		double threshold = 1.0;
		if (formatHasPublishedAngle (format))
		{
			// The threshold is what the technique itself has at its published
			// edge, taken from its own standard array and not from the one being
			// asked about: taking it from the array itself would make the answer
			// the published angle whatever was changed, which is circular.
			ArrayGeometry standard;
			standard.spacingMeters = formatPrescribedSpacingCm (format) / 100.0;
			standard.halfAngleDegrees = formatHalfAngleDegrees (format);
			standard.patternA = formatPatternA (format);

			const double edge =
			    cueSumAtAngle (standard, formatPublishedHalfAngle (format), reference, speedOfSound);
			if (edge > 1.0e-6)
				threshold = edge;
		}

		double low = 0.0;
		double high = 180.0;

		for (int i = 0; i < 50; ++i)
		{
			const double mid = 0.5 * (low + high);
			if (cueSumAtAngle (array, mid, reference, speedOfSound) >= threshold)
				high = mid;
			else
				low = mid;
		}

		return 2.0 * high;
	}
}

//------------------------------------------------------------------------
/** The polar patterns a microphone can be set to, in the order of the list
	parameter. The coefficient is that of a + (1 - a) * cos(angle): 1 is omni,
	0.5 cardioid, 0 figure of eight - which reaches behind with the opposite
	sign, as a figure of eight really does. */
enum PatternList
{
	kPatternOmni = 0,
	kPatternSubcardioid,
	kPatternCardioid,
	kPatternSupercardioid,
	kPatternFigureOfEight,
	kPatternCount
};

/** Coefficient of a list entry. */
inline double patternAForListIndex (int index)
{
	switch (index)
	{
		case kPatternOmni: return 1.0;
		case kPatternSubcardioid: return 0.7;
		case kPatternCardioid: return 0.5;
		case kPatternSupercardioid: return 0.375;
		case kPatternFigureOfEight: return 0.0;
		default: return 0.5;
	}
}

/** The list entry a coefficient came from, for filling the list in. */
inline int patternListIndexForA (double patternA)
{
	int best = kPatternCardioid;
	double bestDistance = 1.0e9;
	for (int i = 0; i < kPatternCount; ++i)
	{
		const double distance = std::fabs (patternAForListIndex (i) - patternA);
		if (distance < bestDistance)
		{
			bestDistance = distance;
			best = i;
		}
	}
	return best;
}

/** Entry name of a pattern. */
inline const char* patternName (int index)
{
	switch (index)
	{
		case kPatternOmni: return "Omni";
		case kPatternSubcardioid: return "Subcardioid";
		case kPatternCardioid: return "Cardioid";
		case kPatternSupercardioid: return "Supercardioid";
		case kPatternFigureOfEight: return "Figure-8";
		default: return "?";
	}
}

//------------------------------------------------------------------------
/** The list entry a normalized parameter value stands for. */
inline int formatFromNormalized (double normalized)
{
	if (normalized <= 0.0)
		return kFormatAB;
	if (normalized >= 1.0)
		return kFormatCount - 1;

	const int index = static_cast<int> (normalized * (kFormatCount - 1) + 0.5);
	if (index < 0)
		return kFormatAB;
	return index >= kFormatCount ? kFormatCount - 1 : index;
}

/** True when the technique leaves the angle its microphones are aimed at to the
	user. A coincident pair has a fixed one - it is part of what XY is - so it is
	not offered to be changed. */
inline bool formatAllowsHalfAngle (int format)
{
	return format != kFormatXY;
}

/** The polar patterns a technique is done with. XY is a pair of cardioids or a
	pair of figure of eights; the aimed cardioid techniques take the cardioid
	family; a plain pair and a tree are omnis. */
inline bool formatAllowsPattern (int format, int patternIndex)
{
	switch (format)
	{
		case kFormatXY:
			return patternIndex == kPatternCardioid || patternIndex == kPatternFigureOfEight;

		case kFormatORTF:
		case kFormatNOS:
		case kFormatDIN:
			return patternIndex == kPatternSubcardioid || patternIndex == kPatternCardioid ||
			       patternIndex == kPatternSupercardioid;

		default:
			return patternIndex == kPatternOmni;
	}
}

/** How many patterns a technique is done with. */
inline int formatAllowedPatternCount (int format)
{
	int count = 0;
	for (int i = 0; i < kPatternCount; ++i)
	{
		if (formatAllowsPattern (format, i))
			++count;
	}
	return count;
}

/** True when the technique is done with one pattern only, so that the row has
	nothing to offer. */
inline bool formatPatternIsFixed (int format)
{
	return formatAllowedPatternCount (format) <= 1;
}

/** The pattern of a technique's own list at a slot, counted from the top. */
inline int formatAllowedPatternAt (int format, int slot)
{
	int at = 0;
	for (int i = 0; i < kPatternCount; ++i)
	{
		if (!formatAllowsPattern (format, i))
			continue;
		if (at == slot)
			return i;
		++at;
	}
	return kPatternOmni;
}

/** The slot a pattern sits at in a technique's list, or 0 when it is not in it. */
inline int formatAllowedPatternSlot (int format, int patternIndex)
{
	int at = 0;
	for (int i = 0; i < kPatternCount; ++i)
	{
		if (!formatAllowsPattern (format, i))
			continue;
		if (i == patternIndex)
			return at;
		++at;
	}
	return 0;
}

/** The pattern of a technique's list that a value belongs at. A value that the
	technique is not done with is brought back to the nearest one it is, so the
	audio never uses a microphone that technique does not have. */
inline int formatPatternInUse (int format, int patternIndex)
{
	if (formatAllowsPattern (format, patternIndex))
		return patternIndex;

	const double wanted = patternAForListIndex (patternIndex);
	int best = formatAllowedPatternAt (format, 0);
	double bestDistance = 1.0e9;
	for (int i = 0; i < kPatternCount; ++i)
	{
		if (!formatAllowsPattern (format, i))
			continue;
		const double distance = std::fabs (patternAForListIndex (i) - wanted);
		if (distance < bestDistance)
		{
			bestDistance = distance;
			best = i;
		}
	}
	return best;
}

} // namespace Panorama
} // namespace MyCompanyName
