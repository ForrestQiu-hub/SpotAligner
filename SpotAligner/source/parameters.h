//------------------------------------------------------------------------
// Copyright(c) 2026 Forrest's Plug-ins.
//------------------------------------------------------------------------
// Parameter layout shared by the processor and the edit controller.
//
// Stage 1 of SpotAligner: a single mono audio object sits at a given
// distance from the main microphone, which is the reference point. Its
// signal is delayed by distance / speed of sound so that it lines up with
// the main microphone. The speed of sound is either entered by hand or
// computed from the room temperature and humidity.
//
// The edit controller owns the VST 3 parameter objects, the processor keeps
// the same values in a plain array (a processor is an IComponent and has no
// parameter container). Both use the very same order, so the state streams
// of both sides stay compatible.
//------------------------------------------------------------------------
#pragma once

#include "public.sdk/source/vst/vsteditcontroller.h"
#include "pluginterfaces/vst/vstspeaker.h"
#include "public.sdk/source/vst/vstparameters.h"

#include "formatRules.h"
#include "soundSpeed.h"
#include "spotPanner.h"

#include <algorithm>
#include <atomic>
#include <cmath>

namespace MyCompanyName {

//------------------------------------------------------------------------
// Parameter identifiers. The order is part of the state format and has to
// stay stable once released.
//------------------------------------------------------------------------
enum SpotAlignerParams : Steinberg::Vst::ParamID
{
	kParamBypass = 0,    ///< 1 = bypassed, 0 = processing (the VST 3 convention)
	kParamDistance,      ///< distance from the main microphone in m
	kParamSpeedMode,     ///< 0 = manual speed of sound, 1 = from air conditions
	kParamSpeedOfSound,  ///< manually entered speed of sound in m/s
	kParamTemperature,   ///< air temperature in degC
	kParamHumidity,      ///< relative humidity in %
	kParamPressure,      ///< static air pressure in kPa
	kParamComputedSpeed, ///< read only readout: speed of sound currently in use
	kParamComputedDelay, ///< read only readout: delay currently applied, in ms

	//--- the panner (stage 2) -------------------------------------------
	kParamSpotAngle,      ///< angle of the object around the array, degrees
	kParamMainFormat,     ///< stereo technique of the main array (list)
	kParamAbSpacing,      ///< distance between the outer microphones, cm
	kParamCentreDistance, ///< Decca Tree centre microphone, cm from the AB centre
	kParamSpotPattern,    ///< polar pattern of the object microphone (list)
	kParamSourceHeight,   ///< height of the object above the floor, m
	kParamMainHeight,     ///< height of the main array above the floor, m
	kParamTrackChannels,  ///< read only: output channels the host gives the plug-in
	kParamDelayLeftMs,    ///< read only: delay of the left side, ms
	kParamDelayRightMs,   ///< read only: delay of the right side, ms
	kParamLevelLeftDb,    ///< read only: level of the left side, dB
	kParamLevelRightDb,   ///< read only: level of the right side, dB
	kParamRecordingAngle, ///< read only: stereo recording angle of the array, degrees
	kParamImagePosition,  ///< read only: where the object lands, per cent

	//--- the array without the straight jacket of a name (stage 3) -------
	// A format is a preset: it fills these in when it is chosen, and after that
	// they can be changed. A pair that is spaced like an ORTF but aimed
	// differently is a real technique and could not be expressed before.
	kParamMainHalfAngle,  ///< how far each array microphone is aimed outward, degrees
	kParamMainPattern,    ///< polar pattern of the array microphones (list)
	kParamSpotHint,       ///< read only: what the object microphone's pattern means here
	kParamGeometryNote,   ///< read only: says so when the array was changed by hand
	kParamMeterLeftDb,    ///< read only: what the left side is putting out right now, dB
	kParamMeterRightDb,   ///< read only: what the right side is putting out right now, dB
	kParamSpotAzimuth,     ///< read only: where the object sits in the image, degrees
	kParamSpotDistance,    ///< read only: how far the object is from the array, meters

	kParamCount
};

//------------------------------------------------------------------------
// State format. Both the processor and the edit controller write and read it,
// so the constants live here instead of being repeated in both files.
//
// Version 1 stored the parameters up to and including kParamComputedDelay
// (the distance stage), version 2 added the panner, version 3 the heights.
// A state of an older version keeps the defaults for what it does not carry.
//------------------------------------------------------------------------
constexpr Steinberg::uint32 kStateVersionV1 = 1;
constexpr Steinberg::uint32 kStateVersionV2 = 2;
constexpr Steinberg::uint32 kStateVersionV3 = 3;
constexpr Steinberg::uint32 kStateVersionV4 = 4;
constexpr Steinberg::uint32 kStateVersionV5 = 5;
constexpr Steinberg::uint32 kStateVersionV6 = 6;
constexpr Steinberg::uint32 kStateVersionV7 = 7;
constexpr Steinberg::uint32 kStateVersionV8 = 8;
constexpr Steinberg::uint32 kStateVersion = 9;
constexpr Steinberg::int32 kParamCountV1 = kParamComputedDelay + 1;
constexpr Steinberg::int32 kParamCountV2 = kParamSpotPattern + 1;
/** Version 3 ended before the read only track report, which is not a setting
	and does not have to be carried by older streams. */
constexpr Steinberg::int32 kParamCountV3 = kParamTrackChannels;
/** Version 4 ended before the read only reports next to the display. */
constexpr Steinberg::int32 kParamCountV4 = kParamTrackChannels + 1;
/** Version 5 ended before the array was cut loose from the format names. */
constexpr Steinberg::int32 kParamCountV5 = kParamImagePosition + 1;
/** Version 6 ended before the note that says the array was changed by hand. */
constexpr Steinberg::int32 kParamCountV6 = kParamGeometryNote;
/** Version 7 ended before the live level meters. */
constexpr Steinberg::int32 kParamCountV7 = kParamGeometryNote + 1;
// Version 8 added the two level meter slots; version 9 added the two read only
// parameters that carry the object's place in the image to the display, so that
// the display never has to read a control that may be mid edit.
constexpr Steinberg::int32 kParamCountV8 = kParamMeterRightDb + 1;

/** Number of parameter values a state of that version carries, or -1 when the
	version is not known. */
inline Steinberg::int32 storedParamCount (Steinberg::uint32 version)
{
	if (version == kStateVersionV1)
		return kParamCountV1;
	if (version == kStateVersionV2)
		return kParamCountV2;
	if (version == kStateVersionV3)
		return kParamCountV3;
	if (version == kStateVersionV4)
		return kParamCountV4;
	if (version == kStateVersionV5)
		return kParamCountV5;
	if (version == kStateVersionV6)
		return kParamCountV6;
	if (version == kStateVersionV7)
		return kParamCountV7;
	if (version == kStateVersionV8)
		return kParamCountV8;
	if (version == kStateVersion)
		return kParamCount;
	return -1;
}

//------------------------------------------------------------------------
// Ranges and defaults
//------------------------------------------------------------------------
/** Distance from the main microphone. This is the horizontal part; the two
	heights add the vertical one (see effectiveSlantDistanceMeters). */
constexpr double kMaxDistanceMeters = 18.0;
constexpr double kDefaultDistanceMeters = 2.0;

/** The panorama display stretches the near field, where the objects usually
	are, instead of spacing the whole range evenly: the first of these metres
	get this share of the radius. See Panorama::distanceToRadius. */
constexpr double kNearFieldMeters = 5.0;
constexpr double kNearFieldRadiusShare = 0.3;

/** Highest output channel count the plug-in reports back (it only ever uses
	two, the rest is reported to show what the host is doing). */
constexpr double kMaxReportedOutputChannels = 8.0;
constexpr double kOutputChannelsForStereo = 2.0;

/** How far each microphone of the array may be aimed outward. */
constexpr double kMinMainHalfAngleDegrees = 0.0;
constexpr double kMaxMainHalfAngleDegrees = 90.0;

/** Range the per side levels are reported in, and where a centred object sits. */
constexpr double kMinLevelDb = -60.0;
constexpr double kMaxLevelDb = 12.0;
constexpr double kDefaultLevelDb = -3.0103;

/** Height above the floor. Only the difference between the two matters for
	the distance, so both share a range. */
constexpr double kMinHeightMeters = 0.0;
constexpr double kMaxHeightMeters = 10.0;
constexpr double kDefaultSourceHeightMeters = 0.0;
constexpr double kDefaultMainHeightMeters = 0.0;

constexpr double kMinSpeedOfSound = 300.0;
constexpr double kMaxSpeedOfSound = 400.0;
constexpr double kDefaultSpeedOfSound = 343.2; ///< dry air at 20 degC

constexpr double kMinTemperatureC = -20.0;
constexpr double kMaxTemperatureC = 60.0;
constexpr double kDefaultTemperatureC = 20.0;

constexpr double kMinHumidityPct = 1.0;
constexpr double kMaxHumidityPct = 100.0;
constexpr double kDefaultHumidityPct = 50.0;

constexpr double kMinPressureKPa = 80.0;
constexpr double kMaxPressureKPa = 110.0;
constexpr double kDefaultPressureKPa = SoundSpeed::kStandardPressureKPa;

/** Displayed range of the "delay in use" readout (ms). The longest delay this
	plug-in can produce is kMaxDistanceMeters / kMinimumSpeedOfSound seconds. */
constexpr double kMinDelayMs = 0.0;
constexpr double kMaxDelayMs = 400.0;

constexpr Steinberg::Vst::ParamValue kSpeedModeManual = 0.0;
constexpr Steinberg::Vst::ParamValue kSpeedModeAirConditions = 1.0;

//--- the panner ---------------------------------------------------------
/** Angle of the object around the main array. The display will become a full
	circle, so the range covers the whole turn. */
constexpr double kMinSpotAngleDegrees = -180.0;
constexpr double kMaxSpotAngleDegrees = 180.0;
constexpr double kDefaultSpotAngleDegrees = 0.0;

/** Distance between the outer microphones, in cm. It has to reach from the
	17 cm of an ORTF pair up to the 3 m of a Decca Tree. */
constexpr double kMinAbSpacingCm = 10.0;
constexpr double kMaxAbSpacingCm = 300.0;
constexpr double kDefaultAbSpacingCm = 40.0;

/** Decca Tree centre microphone, in cm from the centre of the pair. */
constexpr double kMinCentreDistanceCm = 20.0;
constexpr double kMaxCentreDistanceCm = 200.0;
constexpr double kDefaultCentreDistanceCm = 100.0;

/** Entry count of the pattern list. The order of the entries is the order of
	the list the controller registers in createParameters. The format list has
	its count next to the format itself, in formatRules.h: it used to be here as
	well, with a stale seven, and the copy shadowed the real one - which shifted
	every format by one entry. */
constexpr int32_t kSpotPatternCount = 5;

/** Normalized value of a list entry. */
inline double listValue (int32_t index, int32_t count)
{
	if (count < 2)
		return 0.0;
	return static_cast<double> (index) / static_cast<double> (count - 1);
}

//------------------------------------------------------------------------
/** State that the processor and the edit controller share. Both live in the
	same module, so a plain variable is enough. The processor publishes the
	sample rate, which the controller needs for the delay readout. */
inline std::atomic<double>& moduleSampleRate ()
{
	static std::atomic<double> sampleRate {44100.0};
	return sampleRate;
}

//------------------------------------------------------------------------
/** Plain value range of one parameter. */
struct ParamRange
{
	double minPlain;
	double maxPlain;
};

/** Ranges of every parameter, indexed by SpotAlignerParams. */
inline const ParamRange* paramRanges ()
{
	static const ParamRange ranges[kParamCount] = {
	    {0.0, 1.0},                       // kParamBypass
	    {0.0, kMaxDistanceMeters},        // kParamDistance
	    {0.0, 1.0},                       // kParamSpeedMode
	    {kMinSpeedOfSound, kMaxSpeedOfSound}, // kParamSpeedOfSound
	    {kMinTemperatureC, kMaxTemperatureC}, // kParamTemperature
	    {kMinHumidityPct, kMaxHumidityPct},   // kParamHumidity
	    {kMinPressureKPa, kMaxPressureKPa},   // kParamPressure
	    {kMinSpeedOfSound, kMaxSpeedOfSound}, // kParamComputedSpeed
	    {kMinDelayMs, kMaxDelayMs},           // kParamComputedDelay
	    {kMinSpotAngleDegrees, kMaxSpotAngleDegrees}, // kParamSpotAngle
	    {0.0, Panorama::kMainFormatCount - 1.0},      // kParamMainFormat (index)
	    {kMinAbSpacingCm, kMaxAbSpacingCm},           // kParamAbSpacing
	    {kMinCentreDistanceCm, kMaxCentreDistanceCm}, // kParamCentreDistance
	    {0.0, Panorama::kPatternCount - 1.0},         // kParamSpotPattern (index)
	    {kMinHeightMeters, kMaxHeightMeters},         // kParamSourceHeight
	    {kMinHeightMeters, kMaxHeightMeters},         // kParamMainHeight
	    {0.0, kMaxReportedOutputChannels},            // kParamTrackChannels
	    {0.0, kMaxDelayMs},                           // kParamDelayLeftMs
	    {0.0, kMaxDelayMs},                           // kParamDelayRightMs
	    {kMinLevelDb, kMaxLevelDb},                   // kParamLevelLeftDb
	    {kMinLevelDb, kMaxLevelDb},                   // kParamLevelRightDb
	    {0.0, 360.0},                                 // kParamRecordingAngle
	    {-200.0, 200.0},                              // kParamImagePosition
	    {kMinMainHalfAngleDegrees, kMaxMainHalfAngleDegrees}, // kParamMainHalfAngle
	    {0.0, Panorama::kPatternCount - 1.0},         // kParamMainPattern (index)
	    {0.0, Panorama::kPatternCount - 1.0},         // kParamSpotHint (pattern index)
	    {0.0, 1.0},                                   // kParamGeometryNote (read only)
	    {kMinLevelDb, kMaxLevelDb},                   // kParamMeterLeftDb
	    {kMinLevelDb, kMaxLevelDb},                   // kParamMeterRightDb
	    {kMinSpotAngleDegrees, kMaxSpotAngleDegrees},   // kParamSpotAzimuth (read only)
	    {0.0, kMaxDistanceMeters},                      // kParamSpotDistance (read only)
	};
	return ranges;
}

/** Default normalized value of every parameter, indexed by SpotAlignerParams. */
inline const double* defaultNormalizedValues ()
{
	static const double defaults[kParamCount] = {
	    0.0,                                                    // kParamBypass (not bypassed)
	    (kDefaultDistanceMeters - 0.0) / (kMaxDistanceMeters - 0.0), // kParamDistance
	    kSpeedModeAirConditions,                                // kParamSpeedMode
	    (kDefaultSpeedOfSound - kMinSpeedOfSound) / (kMaxSpeedOfSound - kMinSpeedOfSound), // kParamSpeedOfSound
	    (kDefaultTemperatureC - kMinTemperatureC) / (kMaxTemperatureC - kMinTemperatureC), // kParamTemperature
	    (kDefaultHumidityPct - kMinHumidityPct) / (kMaxHumidityPct - kMinHumidityPct),     // kParamHumidity
	    (kDefaultPressureKPa - kMinPressureKPa) / (kMaxPressureKPa - kMinPressureKPa),     // kParamPressure
	    0.0,                                                    // kParamComputedSpeed (read only)
	    0.0,                                                    // kParamComputedDelay (read only)
	    (kDefaultSpotAngleDegrees - kMinSpotAngleDegrees) /
	        (kMaxSpotAngleDegrees - kMinSpotAngleDegrees),       // kParamSpotAngle
	    0.0,                                                     // kParamMainFormat (AB)
	    (kDefaultAbSpacingCm - kMinAbSpacingCm) / (kMaxAbSpacingCm - kMinAbSpacingCm), // kParamAbSpacing
	    (kDefaultCentreDistanceCm - kMinCentreDistanceCm) /
	        (kMaxCentreDistanceCm - kMinCentreDistanceCm),       // kParamCentreDistance
	    2.0 / (Panorama::kPatternCount - 1.0),                   // kParamSpotPattern (cardioid)
	    (kDefaultSourceHeightMeters - kMinHeightMeters) / (kMaxHeightMeters - kMinHeightMeters), // kParamSourceHeight
	    (kDefaultMainHeightMeters - kMinHeightMeters) / (kMaxHeightMeters - kMinHeightMeters),   // kParamMainHeight
	    kOutputChannelsForStereo / kMaxReportedOutputChannels,    // kParamTrackChannels
	    0.0,                                                     // kParamDelayLeftMs
	    0.0,                                                     // kParamDelayRightMs
	    (kDefaultLevelDb - kMinLevelDb) / (kMaxLevelDb - kMinLevelDb),   // kParamLevelLeftDb
	    (kDefaultLevelDb - kMinLevelDb) / (kMaxLevelDb - kMinLevelDb),   // kParamLevelRightDb
	    0.0,                                                     // kParamRecordingAngle
	    0.0,                                                     // kParamImagePosition
	    0.0,                                                     // kParamMainHalfAngle (AB: parallel)
	    0.0,                                                     // kParamMainPattern (omni)
	    0.0,                                                     // kParamSpotHint (read only)
	    0.0,                                                     // kParamGeometryNote (read only)
	    (kMinLevelDb - kMinLevelDb) / (kMaxLevelDb - kMinLevelDb), // kParamMeterLeftDb
	    (kMinLevelDb - kMinLevelDb) / (kMaxLevelDb - kMinLevelDb), // kParamMeterRightDb
	    0.0,                                                     // kParamSpotAzimuth (read only)
	    0.0,                                                     // kParamSpotDistance (read only)
	};
	return defaults;
}

/** Converts a normalized parameter value into its plain (physical) value. */
inline double toPlainValue (Steinberg::int32 paramId, double normalizedValue)
{
	if (paramId < 0 || paramId >= kParamCount)
		return 0.0;

	const ParamRange& range = paramRanges ()[paramId];
	return range.minPlain + normalizedValue * (range.maxPlain - range.minPlain);
}

/** Converts a plain (physical) value into a normalized parameter value. */
inline double toNormalizedValue (Steinberg::int32 paramId, double plainValue)
{
	if (paramId < 0 || paramId >= kParamCount)
		return 0.0;

	const ParamRange& range = paramRanges ()[paramId];
	const double span = range.maxPlain - range.minPlain;
	if (span <= 0.0)
		return 0.0;

	double normalized = (plainValue - range.minPlain) / span;
	if (normalized < 0.0)
		normalized = 0.0;
	else if (normalized > 1.0)
		normalized = 1.0;
	return normalized;
}

//------------------------------------------------------------------------
/** Speed of sound (m/s) that a complete set of normalized parameter values
	results in. The array is indexed by SpotAlignerParams. */
inline double effectiveSpeedOfSound (const double* normalizedValues)
{
	if (!normalizedValues)
		return kDefaultSpeedOfSound;

	if (normalizedValues[kParamSpeedMode] < 0.5) // manual entry
		return toPlainValue (kParamSpeedOfSound, normalizedValues[kParamSpeedOfSound]);

	const double temperatureC = toPlainValue (kParamTemperature, normalizedValues[kParamTemperature]);
	const double humidityPct = toPlainValue (kParamHumidity, normalizedValues[kParamHumidity]);
	const double pressureKPa = toPlainValue (kParamPressure, normalizedValues[kParamPressure]);

	return SoundSpeed::fromAirConditions (temperatureC, humidityPct, pressureKPa);
}

//------------------------------------------------------------------------
/** The first number in a text, or -1 when there is none.

	The editor hands a control the text of a parameter and never a number: for
	a CTextLabel it only calls setText with the string the controller formats
	(see updateControlValue in vst3editor.cpp). A view that has to act on a
	value therefore reads it out of that text, which is the one path the host
	keeps up to date by itself. */
inline int firstNumberIn (const char* text)
{
	if (!text)
		return -1;

	for (const char* p = text; *p != 0; ++p)
	{
		if (*p < '0' || *p > '9')
			continue;

		int value = 0;
		for (; *p >= '0' && *p <= '9'; ++p)
			value = value * 10 + (*p - '0');

		return value;
	}
	return -1;
}

//------------------------------------------------------------------------
/** Number of channels a VST3 speaker arrangement stands for.

	The mono layout has a speaker bit of its own, far above the stereo ones
	(kSpeakerM is bit 19, kStereo is bits 0 and 1), so it cannot be counted by
	looking at the first few bits: a mono arrangement would come out as no
	channels at all. */
inline int channelCountOf (Steinberg::Vst::SpeakerArrangement arrangement)
{
	if (arrangement == Steinberg::Vst::SpeakerArr::kMono)
		return 1;
	if (arrangement == Steinberg::Vst::SpeakerArr::kStereo)
		return 2;

	int channels = 0;
	for (int i = 0; i < 32; ++i)
	{
		if ((arrangement & (static_cast<Steinberg::Vst::SpeakerArrangement> (1) << i)) != 0)
			++channels;
	}
	return channels;
}

//------------------------------------------------------------------------
/** True when the track the plug-in sits on can carry the stereo image it
	produces.

	The number is what the host actually gave the plug-in, which is the only
	thing a plug-in can know about its track and the same question in every
	host: REAPER always hands over two channels whatever the track holds, so
	nothing is reported there, while a host that runs the plug-in in a mono
	chain hands over one and gets the warning. */
inline bool trackCarriesStereo (const double* normalizedValues)
{
	if (!normalizedValues)
		return true;

	return toPlainValue (kParamTrackChannels, normalizedValues[kParamTrackChannels]) >=
	       kOutputChannelsForStereo;
}

//------------------------------------------------------------------------
/** The main array a set of parameters describes.

	This is the one place where parameters become geometry, shared by the audio
	path and by everything the editor displays, so that the two can never
	disagree about what the array is. */
inline Panorama::ArrayGeometry arrayGeometryFromParameters (const double* normalizedValues)
{
	Panorama::ArrayGeometry array;
	if (!normalizedValues)
		return array;

	array.heightMeters = toPlainValue (kParamMainHeight, normalizedValues[kParamMainHeight]);

	const int format =
	    static_cast<int> (toPlainValue (kParamMainFormat, normalizedValues[kParamMainFormat]) + 0.5);

	// The three values below are what the array really is. A format is only a
	// preset that fills them in - the controller does that when the format
	// changes - so a pair spaced like an ORTF but aimed differently, which is a
	// real technique, is expressible; while they were looked up from the name
	// it was not.
	array.spacingMeters = toPlainValue (kParamAbSpacing, normalizedValues[kParamAbSpacing]) / 100.0;

	// A technique that fixes one of the two keeps it: a coincident pair has the
	// angle it has, and a plain pair or a tree is done with omnis, whatever the
	// field happens to hold.
	array.halfAngleDegrees =
	    Panorama::formatAllowsHalfAngle (format)
	        ? toPlainValue (kParamMainHalfAngle, normalizedValues[kParamMainHalfAngle])
	        : Panorama::formatHalfAngleDegrees (format);

	const int wantedPattern =
	    static_cast<int> (toPlainValue (kParamMainPattern, normalizedValues[kParamMainPattern]) + 0.5);
	array.patternA =
	    Panorama::patternAForListIndex (Panorama::formatPatternInUse (format, wantedPattern));

	// the centre microphone of a tree, and nothing for the other techniques
	if (Panorama::formatUsesCentreDistance (format))
	{
		array.centreDistanceMeters =
		    toPlainValue (kParamCentreDistance, normalizedValues[kParamCentreDistance]) / 100.0;
	}

	return array;
}

/** Where the object stands, from the same set of parameters. */
inline Panorama::ObjectPlacement objectPlacementFromParameters (const double* normalizedValues)
{
	Panorama::ObjectPlacement object;
	if (!normalizedValues)
		return object;

	object.distanceMeters = toPlainValue (kParamDistance, normalizedValues[kParamDistance]);
	object.azimuthDegrees = toPlainValue (kParamSpotAngle, normalizedValues[kParamSpotAngle]);
	object.heightMeters = toPlainValue (kParamSourceHeight, normalizedValues[kParamSourceHeight]);

	return object;
}

/** Speed of sound that is in use, never below what the calculation can use. */
inline double usableSpeedOfSound (const double* normalizedValues)
{
	const double speed = effectiveSpeedOfSound (normalizedValues);
	return speed < kMinSpeedOfSound ? kMinSpeedOfSound : speed;
}

/** Stereo recording angle of the array the parameters describe, in degrees. */
inline double recordingAngleDegreesFromParameters (const double* normalizedValues)
{
	const Panorama::ArrayGeometry array = arrayGeometryFromParameters (normalizedValues);
	const int format =
	    static_cast<int> (toPlainValue (kParamMainFormat, normalizedValues[kParamMainFormat]) + 0.5);

	return Panorama::formatRecordingAngleDegrees (format, array, usableSpeedOfSound (normalizedValues));
}

//------------------------------------------------------------------------
/** Distance between the object and the main microphone, in metres.

	The distance parameter is the horizontal part, the two heights give the
	vertical one: only their difference changes the length, which is why both
	are measured from the same floor. This is the distance the delay is made
	of, and it is computed here only, so that the audio path and the readout
	can never disagree. */
inline double effectiveSlantDistanceMeters (const double* normalizedValues)
{
	if (!normalizedValues)
		return 0.0;

	const double horizontal = toPlainValue (kParamDistance, normalizedValues[kParamDistance]);
	const double sourceHeight = toPlainValue (kParamSourceHeight, normalizedValues[kParamSourceHeight]);
	const double mainHeight = toPlainValue (kParamMainHeight, normalizedValues[kParamMainHeight]);
	const double rise = sourceHeight - mainHeight;

	return std::sqrt (horizontal * horizontal + rise * rise);
}

/** Longest distance the parameters can produce, which is what the delay line
	has to hold. */
inline double maxSlantDistanceMeters ()
{
	const double rise = kMaxHeightMeters - kMinHeightMeters;
	return std::sqrt (kMaxDistanceMeters * kMaxDistanceMeters + rise * rise);
}

//------------------------------------------------------------------------
/** Delay (in milliseconds) that lines the object up with the main microphone
	for a complete set of normalized parameter values. This is what the current
	distance, heights and speed of sound result in, independent of the alignment
	switch, so the readout keeps showing the value while the plug-in is
	bypassed. */
inline double effectiveDelayMs (const double* normalizedValues)
{
	if (!normalizedValues)
		return 0.0;

	const double distanceMeters = effectiveSlantDistanceMeters (normalizedValues);

	const double speedOfSound = std::max (kMinSpeedOfSound, effectiveSpeedOfSound (normalizedValues));

	return distanceMeters / speedOfSound * 1000.0;
}

//------------------------------------------------------------------------
} // namespace MyCompanyName
