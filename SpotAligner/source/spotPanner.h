//------------------------------------------------------------------------
// Copyright(c) 2026 Forrest's Plug-ins.
//------------------------------------------------------------------------
// Where the object has to go so that it sits in the main pair the way the
// array would have picked it up.
//
// The main array is two microphones standing apart and aimed outwards. A source
// somewhere in front of it reaches the two of them along different paths, so it
// arrives at slightly different times and at different levels. Placing a spot
// microphone means reproducing both: its left channel is delayed and weighted
// like the array's left microphone would have heard it, its right channel like
// the right one.
//
// Kept free of any VSTGUI or VST types, so that a headless test can check it.
//
//   * Coincident arrays (XY, MS) have no spacing, so both sides are delayed by
//     the same amount and only the levels differ - plain amplitude panning,
//     which is exactly right for a coincident pair.
//   * Spaced arrays (AB, ORTF, NOS, DIN) add a real time difference, up to
//     several milliseconds for a wide AB, which is what makes a spot microphone
//     mixed in without it sound like a second source.
//------------------------------------------------------------------------
#pragma once

#include "panoramaGeometry.h"

#include <cmath>

namespace MyCompanyName {
namespace Panorama {

// kPi comes from panoramaGeometry.h, so that the project has one definition.

//------------------------------------------------------------------------
/** The main array, as far as the plug-in has to know it. */
struct ArrayGeometry
{
	/** Distance between the two outer microphones, in metres. Zero means the
	    array is coincident. */
	double spacingMeters = 0.0;

	/** How far each microphone is aimed away from the centre, in degrees. A
	    source that sits where one of them points is picked up by that one at
	    full level and by the other one off its axis. */
	double halfAngleDegrees = 0.0;

	/** Polar response of the array microphones, as the coefficient a of
	    a + (1 - a) * cos(angle): 1 is omni, 0.5 cardioid, 0 figure of eight. */
	double patternA = 0.5;

	/** Height of the array above the floor, in metres. */
	double heightMeters = 0.0;

	/** Distance from the left to right line forward to a centre microphone, in
	    metres. Zero means the array has none, which is every technique except
	    the Decca Tree. The centre microphone stands in the middle of the array
	    and in front of it, so its path to a source differs from both others by
	    more than the spacing alone would give. */
	double centreDistanceMeters = 0.0;
};

/** Where the object stands, seen from the middle of the array. */
struct ObjectPlacement
{
	/** Horizontal distance from the middle of the array, in metres. */
	double distanceMeters = 0.0;

	/** Angle across the array, in degrees: 0 is straight ahead, positive to
	    the right. */
	double azimuthDegrees = 0.0;

	/** Height above the same floor the array height is measured from. */
	double heightMeters = 0.0;
};

/** What one side of the output has to do with the object. */
struct ChannelSetup
{
	double delaySeconds = 0.0;
	double gain = 0.0;
};

struct StereoSetup
{
	ChannelSetup left;
	ChannelSetup right;

	/** A centre microphone, fed to both sides equally. Its gain is what one
	    side of the output receives, so the two together carry its power. */
	ChannelSetup centre;
};

//------------------------------------------------------------------------
/** Distance from the object to one microphone of the array, in metres.

	@param left  true for the microphone on the left of the array. */
inline double distanceToMicrophone (const ArrayGeometry& array, const ObjectPlacement& object, bool left)
{
	const double radians = object.azimuthDegrees * kPi / 180.0;

	// the microphones stand on the left to right axis through the middle of the
	// array, the object somewhere in front of it
	const double side = left ? -0.5 : 0.5;
	const double across = object.distanceMeters * std::sin (radians) - side * array.spacingMeters;
	const double forward = object.distanceMeters * std::cos (radians);
	const double rise = object.heightMeters - array.heightMeters;

	return std::sqrt (across * across + forward * forward + rise * rise);
}

/** Distance from the object to the centre microphone of an array, in metres.
	It stands in the middle and centreDistanceMeters in front of the line the
	other two are on. */
inline double distanceToCentreMicrophone (const ArrayGeometry& array, const ObjectPlacement& object)
{
	const double radians = object.azimuthDegrees * kPi / 180.0;

	const double across = object.distanceMeters * std::sin (radians);
	const double forward = object.distanceMeters * std::cos (radians) - array.centreDistanceMeters;
	const double rise = object.heightMeters - array.heightMeters;

	return std::sqrt (across * across + forward * forward + rise * rise);
}

/** How far off its axis a microphone sees the object, in degrees. */
inline double offAxisDegrees (const ArrayGeometry& array, const ObjectPlacement& object, bool left)
{
	const double axis = left ? -array.halfAngleDegrees : array.halfAngleDegrees;
	return std::fabs (object.azimuthDegrees - axis);
}

/** Response of a microphone with that polar pattern, 1 on axis. */
inline double polarResponse (double patternA, double angleDegrees)
{
	return patternA + (1.0 - patternA) * std::cos (angleDegrees * kPi / 180.0);
}

//------------------------------------------------------------------------
/** Where each side has to be delayed and how loud it has to be.

	The gains are normalised so that their powers add up to one: the object
	keeps the loudness it had whatever its position, and a source straight ahead
	sits at equal power in both channels, which is where a centred mono source
	belongs. */
inline StereoSetup stereoSetup (const ArrayGeometry& array, const ObjectPlacement& object, double speedOfSound)
{
	StereoSetup setup;

	if (speedOfSound <= 0.0)
	{
		setup.left.gain = 0.70710678118654752;
		setup.right.gain = setup.left.gain;
		return setup;
	}

	setup.left.delaySeconds = distanceToMicrophone (array, object, true) / speedOfSound;
	setup.right.delaySeconds = distanceToMicrophone (array, object, false) / speedOfSound;

	// Each microphone hears the object at a level that falls with its own
	// distance, on top of how far off its axis the object is. With a wide array
	// that stands close to the object this is several dB, and it is the level
	// difference a wide AB is known for: at 3 m of spacing and 4 m of distance
	// the two paths differ by more than 4 dB, while at 8 m of distance the same
	// array is within a fraction of a dB. For a coincident array both distances
	// are the same and the term cancels, which leaves the polar patterns as the
	// only source of a level difference there.
	const double leftDistance = distanceToMicrophone (array, object, true);
	const double rightDistance = distanceToMicrophone (array, object, false);

	setup.left.gain = polarResponse (array.patternA, offAxisDegrees (array, object, true)) /
	                  (leftDistance > 1.0e-6 ? leftDistance : 1.0e-6);
	setup.right.gain = polarResponse (array.patternA, offAxisDegrees (array, object, false)) /
	                   (rightDistance > 1.0e-6 ? rightDistance : 1.0e-6);

	// The centre microphone of a tree. It is pushed like the outer pair - the
	// three faders of a tree stand together - so its weight is the mean of
	// theirs rather than whatever its shorter path would make it. It goes to
	// both sides at that weight, which is what a centre microphone panned to
	// the middle does.
	double centrePower = 0.0;
	if (array.centreDistanceMeters > 0.0)
	{
		const double centreDistance = distanceToCentreMicrophone (array, object);
		setup.centre.delaySeconds = centreDistance / speedOfSound;

		const double outerMean = std::sqrt (setup.left.gain * setup.right.gain);
		setup.centre.gain = outerMean > 1.0e-9 ? outerMean : 0.0;

		// it stands in both sides, so its power counts twice
		centrePower = 2.0 * setup.centre.gain * setup.centre.gain;
	}

	// The power of the whole thing, counting the centre once per side
	const double power = setup.left.gain * setup.left.gain + setup.right.gain * setup.right.gain + centrePower;
	if (power > 0.0)
	{
		const double scale = 1.0 / std::sqrt (power);
		setup.left.gain *= scale;
		setup.right.gain *= scale;
		setup.centre.gain *= scale;
	}
	else
	{
		// fully off the back of both microphones: fall back to the centre
		setup.left.gain = 0.70710678118654752;
		setup.right.gain = setup.left.gain;
		setup.centre.gain = 0.0;
	}

	return setup;
}

//------------------------------------------------------------------------
/** The two cues of a placement added up the way the image position uses them:
	right louder and left later both push to the right, and each counts for the
	share of a full shift that its published constant gives it.

	A centre microphone stands in both channels, so what the two sides differ by
	is diluted by it twice over: the level difference it adds to both sides
	shrinks the ratio between them, and the part of each channel that is the
	centre carries no time difference at all. Both are counted here, which is
	why a tree with a centre microphone maps the same source to a place closer
	to the middle than its outer pair alone would. */
inline double cueSum (const StereoSetup& setup)
{
	// How much each side receives. A directional microphone whose source is
	// past its null answers with the opposite sign - a figure of eight behind
	// itself, a supercardioid past about 127 degrees - and two sides that
	// answer with opposite signs are not a small difference but the far end of
	// the image, which is what a Blumlein pair at ninety degrees is. The audio
	// path is what carries the inversion itself.
	const double channelLeft = setup.left.gain + setup.centre.gain;
	const double channelRight = setup.right.gain + setup.centre.gain;

	if (std::fabs (channelLeft) <= 1.0e-9 || std::fabs (channelRight) <= 1.0e-9)
		return 1.0e9; // one side is at its null: past the extreme

	const double ratio = channelRight / channelLeft;
	const double levelDb = ratio > 0.0 ? 20.0 * std::log10 (ratio) : 60.0;
	const double timeMs = (setup.left.delaySeconds - setup.right.delaySeconds) * 1000.0;

	// The share of a channel that is the outer pair, and so the only part that
	// can carry a time difference. The two gains can have opposite signs - a
	// coincident pair of figure of eights at ninety degrees is exactly that,
	// and their sum is zero there - so the share is a magnitude over a
	// magnitude, and a denominator that vanishes leaves no share at all instead
	// of a division by zero. That zero over zero turned the whole answer into a
	// NaN and made the solve report the full front for a Blumlein pair.
	const double outerSum = setup.left.gain + setup.right.gain;
	const double denominator = outerSum + 2.0 * setup.centre.gain;

	double outerShare = 0.0;
	if (denominator > 1.0e-9)
		outerShare = std::fabs (outerSum) / denominator;

	return levelDb / 18.0 + timeMs / 1.5 * outerShare;
}

//------------------------------------------------------------------------
/** The cue sum of an array for a source at one angle, at a given distance.
	Used to find the recording angle of an array whose centre microphone puts
	the answer out of reach of a closed form. */
inline double cueSumAtAngle (const ArrayGeometry& array, double azimuthDegrees, double distanceMeters,
                             double speedOfSound)
{
	ObjectPlacement object;
	object.distanceMeters = distanceMeters;
	object.azimuthDegrees = azimuthDegrees;
	object.heightMeters = array.heightMeters;

	return cueSum (stereoSetup (array, object, speedOfSound));
}

//------------------------------------------------------------------------
/** Where the object lands in the stereo image.

	0 is the middle, +1 one loudspeaker, -1 the other.

	The two cues are combined the way equivalence stereophony does it: a level
	difference of 18 dB and a time difference of 1.5 ms each mean a full shift
	on their own, and the array's own recording angle sets the scale so that a
	source at its edge comes out at exactly 1. Both end points are taken from
	the published tables - an omni AB reaches its edge through time alone, a
	coincident cardioid pair through level alone - so the curve is exact for
	those two kinds of array and an approximation in between for the ones that
	use both cues.

	Beyond the recording angle the result keeps growing, and how it grows is
	what makes the arrays differ: a spaced pair saturates because its time
	difference cannot grow past the spacing, while an aimed cardioid pair keeps
	going because its level difference has no such limit. */
inline double imagePosition (const ArrayGeometry& array, const ObjectPlacement& object,
                             double recordingAngleDegrees, double speedOfSound)
{
	const StereoSetup here = stereoSetup (array, object, speedOfSound);

	ObjectPlacement edge = object;
	edge.azimuthDegrees = recordingAngleDegrees / 2.0;
	const StereoSetup atEdge = stereoSetup (array, edge, speedOfSound);

	const double edgeValue = cueSum (atEdge);
	if (std::fabs (edgeValue) < 1.0e-9)
		return 0.0;

	return cueSum (here) / edgeValue;
}

//------------------------------------------------------------------------
} // namespace Panorama
} // namespace MyCompanyName
