//------------------------------------------------------------------------
// Copyright(c) 2026 Forrest's Plug-ins.
//------------------------------------------------------------------------

#include "controller.h"
#include "cids.h"
#include "formatRules.h"
#include "arrayParameters.h"
#include "parameters.h"
#include "vstgui/plugin-bindings/vst3editor.h"

#include "base/source/fstreamer.h"

#include "pluginterfaces/base/ustring.h"

#include <algorithm>
#include <cstring>

using namespace Steinberg;

namespace MyCompanyName {

//------------------------------------------------------------------------
// ParamRef: a parameter that can also parse its own display string.
//
// The SDK converts a value into a string (toString) but converts a string
// back with a plain float scan only. Two of our parameters are not numeric
// ("On"/"Off" for the toggle, the two speed modes for the list), so their
// display string would not survive a round trip through the host. This type
// adds the missing direction.
//------------------------------------------------------------------------
class ParamRef : public Vst::RangeParameter
{
public:
	ParamRef (const Vst::ParameterInfo& info, Vst::ParamValue minPlain, Vst::ParamValue maxPlain,
	          std::initializer_list<const Steinberg::tchar*> entries = {})
	: RangeParameter (info, minPlain, maxPlain), texts (entries)
	{
	}

	bool fromString (const Steinberg::tchar* string, Vst::ParamValue& valueNormalized) const SMTG_OVERRIDE
	{
		if (!string)
			return false;

		// the toggle displays "On" / "Off", the mode list its entries
		if (texts.size () > 0)
		{
			for (size_t i = 0; i < texts.size (); ++i)
			{
				if (equalsIgnoreCase (string, texts[i]))
				{
					valueNormalized = Vst::ParamValue (i) / Vst::ParamValue (texts.size () - 1);
					return true;
				}
			}
		}

		// A field shows its unit ("2.000 m", "101.33 kPa"), and the SDK float
		// scan rejects trailing text, so keep the leading number only. This is
		// what lets the user type both "2" and "2 m".
		Steinberg::tchar number[64];
		int length = 0;
		for (const Steinberg::tchar* p = string; *p != 0 && length < 63; ++p)
		{
			const Steinberg::tchar c = *p;
			const bool numeric = (c >= STR16 ('0') && c <= STR16 ('9')) || c == STR16 ('.') ||
			                     c == STR16 ('-') || c == STR16 ('+') || c == STR16 ('e') || c == STR16 ('E');
			if (!numeric)
			{
				if (length > 0)
					break; // reached the unit
				continue;  // skip leading blanks
			}
			number[length++] = c;
		}
		number[length] = 0;

		if (length == 0)
		{
			// an emptied field falls back to the documented default, so
			// clearing the speed of sound gives 343.2 m/s again
			valueNormalized = info.defaultNormalizedValue;
			return true;
		}

		return RangeParameter::fromString (number, valueNormalized);
	}

	/** A two state parameter shows its state name; otherwise the SDK writes the
	    number only, so the unit is appended by hand and the fields read
	    "2.000 m" instead of "2.000". */
	void toString (Vst::ParamValue valueNormalized, Vst::String128 string) const SMTG_OVERRIDE
	{
		if (!texts.empty () && info.stepCount == 1)
		{
			const size_t index = valueNormalized > 0.5 ? 1 : 0;
			Steinberg::UString (string, str16BufferSize (Vst::String128))
			    .assign (texts[index < texts.size () ? index : 0]);
			return;
		}

		RangeParameter::toString (valueNormalized, string);

		if (info.units[0] == 0)
			return;

		Steinberg::UString text (string, str16BufferSize (Vst::String128));
		text.append (STR16 (" "));
		text.append (info.units);
	}

	/** Default value given as the normalized one. */
	void setDefaultNormalized (Vst::ParamValue normalized)
	{
		info.defaultNormalizedValue = normalized;
		setNormalized (normalized);
	}

	/** Default value given in the physical unit of the parameter. */
	void setDefaultPlain (double plain)
	{
		setDefaultNormalized (toNormalized (plain));
	}

private:
	static bool equalsIgnoreCase (const Steinberg::tchar* a, const Steinberg::tchar* b)
	{
		if (!a || !b)
			return false;

		while (*a && *b)
		{
			const Steinberg::tchar ca = static_cast<Steinberg::tchar> (toupperLowerCase (*a));
			const Steinberg::tchar cb = static_cast<Steinberg::tchar> (toupperLowerCase (*b));
			if (ca != cb)
				return false;
			++a;
			++b;
		}
		return *a == 0 && *b == 0;
	}

	static Steinberg::tchar toupperLowerCase (Steinberg::tchar c)
	{
		if (c >= STR16 ('a') && c <= STR16 ('z'))
			return static_cast<Steinberg::tchar> (c - STR16 ('a') + STR16 ('A'));
		return c;
	}

	std::vector<const Steinberg::tchar*> texts;
};

//------------------------------------------------------------------------
/** Gives a list parameter its default entry. The constructor used here with
	(title, tag, units, flags) does not set defaultNormalizedValue, so it is
	assigned afterwards. */
static void setListDefault (Vst::Parameter& parameter, int32_t index, int32_t count)
{
	const double value = listValue (index, count);
	parameter.getInfo ().defaultNormalizedValue = value;
	parameter.setNormalized (value);
}

//------------------------------------------------------------------------
/** The channel report, formatted as the sentence the editor shows.

	The editor gives a label the text of a parameter and never a number, and it
	asks the controller for that text, so the sentence is made here: the user
	reads it, and the view reads the number back out of it with firstNumberIn. */
class TrackChannelsRef : public Vst::RangeParameter
{
public:
	TrackChannelsRef (const Vst::ParameterInfo& info, Vst::ParamValue minPlain, Vst::ParamValue maxPlain)
	: RangeParameter (info, minPlain, maxPlain)
	{
	}

	void toString (Vst::ParamValue valueNormalized, Vst::String128 string) const SMTG_OVERRIDE
	{
		const int channels = static_cast<int> (toPlain (valueNormalized) + 0.5);

		char text[64];
		if (channels == 1)
			std::snprintf (text, sizeof (text), "Track output: 1 channel");
		else
			std::snprintf (text, sizeof (text), "Track output: %d channels", channels);

		UString wrapper (string, str16BufferSize (Vst::String128));
		wrapper.assign (text);
	}
};

//------------------------------------------------------------------------
/** Creates the parameter objects of this plug-in and puts them into the
	controller's parameter container. The order is part of the state format and
	must match the SpotAlignerParams enumeration. */
static void createParameters (Vst::ParameterContainer& container)
{
	using namespace Vst;

	const int32 automatable = ParameterInfo::kCanAutomate;

	//--- bypass: 1 = bypassed, 0 = processing (VST 3 convention) ---------
	{
		ParameterInfo info {};
		UString (info.title, str16BufferSize (String128)).assign (STR16 ("Bypass"));
		info.flags = automatable | ParameterInfo::kIsBypass;
		info.stepCount = 1;
		info.id = kParamBypass;
		info.unitId = kRootUnitId;

		auto* param = new ParamRef (info, 0.0, 1.0, {STR16 ("Active"), STR16 ("Bypassed")});
		param->setPrecision (0);
		param->setDefaultNormalized (0.0); // processing by default
		container.addParameter (param);
	}

	//--- distance to the main microphone ---------------------------------
	{
		ParameterInfo info {};
		UString (info.title, str16BufferSize (String128)).assign (STR16 ("Distance"));
		UString (info.units, str16BufferSize (String128)).assign (STR16 ("m"));
		info.flags = automatable;
		info.stepCount = kStepCountContinuous;
		info.id = kParamDistance;
		info.unitId = kRootUnitId;

		auto* param = new ParamRef (info, 0.0, kMaxDistanceMeters);
		param->setPrecision (3);
		param->setDefaultPlain (kDefaultDistanceMeters);
		container.addParameter (param);
	}

	//--- where the speed of sound comes from -----------------------------
	container.addParameter (new StringListParameter (STR16 ("Speed Mode"), kParamSpeedMode, nullptr,
	                                                 ParameterInfo::kCanAutomate | ParameterInfo::kIsList));
	if (auto* mode = container.getParameter (kParamSpeedMode))
	{
		auto* list = dynamic_cast<StringListParameter*> (mode);
		if (list)
		{
			list->appendString (STR16 ("Manual"));
			list->appendString (STR16 ("From air conditions"));
		}

		// the speed of sound entered by hand is the default source (entry 0)
		setListDefault (*mode, 0, 2);
	}

	//--- manual speed of sound -------------------------------------------
	{
		ParameterInfo info {};
		UString (info.title, str16BufferSize (String128)).assign (STR16 ("Speed of Sound"));
		UString (info.units, str16BufferSize (String128)).assign (STR16 ("m/s"));
		info.flags = automatable;
		info.stepCount = kStepCountContinuous;
		info.id = kParamSpeedOfSound;
		info.unitId = kRootUnitId;

		auto* param = new ParamRef (info, kMinSpeedOfSound, kMaxSpeedOfSound);
		param->setPrecision (2);
		param->setDefaultPlain (kDefaultSpeedOfSound);
		container.addParameter (param);
	}

	//--- air conditions ---------------------------------------------------
	{
		ParameterInfo info {};
		UString (info.title, str16BufferSize (String128)).assign (STR16 ("Temperature"));
		UString (info.units, str16BufferSize (String128)).assign (STR16 ("\u00B0C"));
		info.flags = automatable;
		info.stepCount = kStepCountContinuous;
		info.id = kParamTemperature;
		info.unitId = kRootUnitId;

		auto* param = new ParamRef (info, kMinTemperatureC, kMaxTemperatureC);
		param->setPrecision (2);
		param->setDefaultPlain (kDefaultTemperatureC);
		container.addParameter (param);
	}
	{
		ParameterInfo info {};
		UString (info.title, str16BufferSize (String128)).assign (STR16 ("Humidity"));
		UString (info.units, str16BufferSize (String128)).assign (STR16 ("%"));
		info.flags = automatable;
		info.stepCount = kStepCountContinuous;
		info.id = kParamHumidity;
		info.unitId = kRootUnitId;

		auto* param = new ParamRef (info, kMinHumidityPct, kMaxHumidityPct);
		param->setPrecision (1);
		param->setDefaultPlain (kDefaultHumidityPct);
		container.addParameter (param);
	}
	{
		ParameterInfo info {};
		UString (info.title, str16BufferSize (String128)).assign (STR16 ("Air Pressure"));
		UString (info.units, str16BufferSize (String128)).assign (STR16 ("kPa"));
		info.flags = automatable;
		info.stepCount = kStepCountContinuous;
		info.id = kParamPressure;
		info.unitId = kRootUnitId;

		auto* param = new ParamRef (info, kMinPressureKPa, kMaxPressureKPa);
		param->setPrecision (3); // keeps the standard pressure 101.325 exact
		param->setDefaultPlain (kDefaultPressureKPa);
		container.addParameter (param);
	}

	//--- read only readouts ----------------------------------------------
	{
		ParameterInfo info {};
		UString (info.title, str16BufferSize (String128)).assign (STR16 ("Speed in Use"));
		UString (info.units, str16BufferSize (String128)).assign (STR16 ("m/s"));
		info.flags = ParameterInfo::kIsReadOnly;
		info.stepCount = kStepCountContinuous;
		info.id = kParamComputedSpeed;
		info.unitId = kRootUnitId;

		auto* param = new ParamRef (info, kMinSpeedOfSound, kMaxSpeedOfSound);
		param->setPrecision (2);
		container.addParameter (param);
	}
	{
		ParameterInfo info {};
		UString (info.title, str16BufferSize (String128)).assign (STR16 ("Delay in Use"));
		UString (info.units, str16BufferSize (String128)).assign (STR16 ("ms"));
		info.flags = ParameterInfo::kIsReadOnly;
		info.stepCount = kStepCountContinuous;
		info.id = kParamComputedDelay;
		info.unitId = kRootUnitId;

		auto* param = new ParamRef (info, kMinDelayMs, kMaxDelayMs);
		param->setPrecision (3);
		container.addParameter (param);
	}

	//--- the panner ------------------------------------------------------
	{
		ParameterInfo info {};
		UString (info.title, str16BufferSize (String128)).assign (STR16 ("Spot Angle"));
		UString (info.units, str16BufferSize (String128)).assign (STR16 ("\u00B0"));
		info.flags = automatable;
		info.stepCount = kStepCountContinuous;
		info.id = kParamSpotAngle;
		info.unitId = kRootUnitId;

		auto* param = new ParamRef (info, kMinSpotAngleDegrees, kMaxSpotAngleDegrees);
		param->setPrecision (1);
		param->setDefaultPlain (kDefaultSpotAngleDegrees);
		container.addParameter (param);
	}
	{
		// AB, ORTF, XY, NOS, DIN - the order is fixed by Panorama::MainFormat in
		// formatRules.h. MS is not offered: its side signal is normally adjusted
		// while mixing, so the pair the plug-in would model is not the pair that
		// ends up in the recording. Decca Tree is not offered either: it needs a
		// third, centre microphone and the plug-in produces two channels.
		auto* format = new StringListParameter (STR16 ("Main Format"), kParamMainFormat, nullptr,
		                                        ParameterInfo::kCanAutomate | ParameterInfo::kIsList);
		format->appendString (STR16 ("AB"));
		format->appendString (STR16 ("ORTF"));
		format->appendString (STR16 ("XY"));
		format->appendString (STR16 ("NOS"));
		format->appendString (STR16 ("DIN"));
		format->appendString (STR16 ("Decca Tree"));
		setListDefault (*format, Panorama::kFormatAB, Panorama::kMainFormatCount);
		container.addParameter (format);

	// the array itself, which the format above only fills in
	addArrayParameters (container);
	}
	{
		ParameterInfo info {};
		UString (info.title, str16BufferSize (String128)).assign (STR16 ("AB Spacing"));
		UString (info.units, str16BufferSize (String128)).assign (STR16 ("cm"));
		info.flags = automatable;
		info.stepCount = kStepCountContinuous;
		info.id = kParamAbSpacing;
		info.unitId = kRootUnitId;

		auto* param = new ParamRef (info, kMinAbSpacingCm, kMaxAbSpacingCm);
		param->setPrecision (1);
		param->setDefaultPlain (kDefaultAbSpacingCm);
		container.addParameter (param);
	}
	{
		ParameterInfo info {};
		UString (info.title, str16BufferSize (String128)).assign (STR16 ("Centre Distance"));
		UString (info.units, str16BufferSize (String128)).assign (STR16 ("cm"));
		info.flags = automatable;
		info.stepCount = kStepCountContinuous;
		info.id = kParamCentreDistance;
		info.unitId = kRootUnitId;

		auto* param = new ParamRef (info, kMinCentreDistanceCm, kMaxCentreDistanceCm);
		param->setPrecision (1);
		param->setDefaultPlain (kDefaultCentreDistanceCm);
		container.addParameter (param);
	}
	{
		// Omni, Subcardioid, Cardioid, Supercardioid, Figure-8
		auto* pattern = new StringListParameter (STR16 ("Spot Pattern"), kParamSpotPattern, nullptr,
		                                         ParameterInfo::kCanAutomate | ParameterInfo::kIsList);
		pattern->appendString (STR16 ("Omni"));
		pattern->appendString (STR16 ("Subcardioid"));
		pattern->appendString (STR16 ("Cardioid"));
		pattern->appendString (STR16 ("Supercardioid"));
		pattern->appendString (STR16 ("Figure-8"));
		setListDefault (*pattern, 2, kSpotPatternCount); // cardioid
		container.addParameter (pattern);
	}
	{
		ParameterInfo info {};
		UString (info.title, str16BufferSize (String128)).assign (STR16 ("Spot Height"));
		UString (info.units, str16BufferSize (String128)).assign (STR16 ("m"));
		info.flags = automatable;
		info.stepCount = kStepCountContinuous;
		info.id = kParamSourceHeight;
		info.unitId = kRootUnitId;

		auto* param = new ParamRef (info, kMinHeightMeters, kMaxHeightMeters);
		param->setPrecision (2);
		param->setDefaultPlain (kDefaultSourceHeightMeters);
		container.addParameter (param);
	}
	{
		ParameterInfo info {};
		UString (info.title, str16BufferSize (String128)).assign (STR16 ("Main Height"));
		UString (info.units, str16BufferSize (String128)).assign (STR16 ("m"));
		info.flags = automatable;
		info.stepCount = kStepCountContinuous;
		info.id = kParamMainHeight;
		info.unitId = kRootUnitId;

		auto* param = new ParamRef (info, kMinHeightMeters, kMaxHeightMeters);
		param->setPrecision (2);
		param->setDefaultPlain (kDefaultMainHeightMeters);
		container.addParameter (param);
	}
	{
		// Not a setting: the processor finds out how many output channels the
		// host really gives it and reports that back through this parameter,
		// which is how the editor learns about it. Two are needed for the
		// stereo image; one means the object is passed through and the editor
		// asks for a track that can carry it.
		//
		// The report is shown as text: the editor gives a label the text of a
		// parameter and never a number, so this is where the sentence the user
		// reads comes from, and the view reads the number back out of it.
		ParameterInfo info {};
		UString (info.title, str16BufferSize (String128)).assign (STR16 ("Track Channels"));
		info.flags = ParameterInfo::kIsReadOnly;
		info.stepCount = static_cast<int32> (kMaxReportedOutputChannels);
		info.id = kParamTrackChannels;
		info.unitId = kRootUnitId;

		auto* param = new TrackChannelsRef (info, 0.0, kMaxReportedOutputChannels);
		param->setPrecision (0);

		// assume a track that can carry the image until the processor reports
		// otherwise, as the default and as the current value, so that the
		// editor does not start out showing a warning that is not there
		param->getInfo ().defaultNormalizedValue = kOutputChannelsForStereo / kMaxReportedOutputChannels;
		param->setNormalized (kOutputChannelsForStereo / kMaxReportedOutputChannels);
		container.addParameter (param);
	}

	// The six numbers next to the display. They are computed by the controller
	// (see updateComputedReadouts) and only read by the editor, which is how a
	// label gets them: a label is handed the text of its parameter and nothing
	// else.
	const struct
	{
		const char* title;
		const char* units;
		int32 id;
		double minPlain;
		double maxPlain;
		int32 precision;
	} readouts[] = {
	    {"Delay L", "ms", kParamDelayLeftMs, 0.0, kMaxDelayMs, 3},
	    {"Delay R", "ms", kParamDelayRightMs, 0.0, kMaxDelayMs, 3},
	    {"Level L", "dB", kParamLevelLeftDb, kMinLevelDb, kMaxLevelDb, 2},
	    {"Level R", "dB", kParamLevelRightDb, kMinLevelDb, kMaxLevelDb, 2},
	    {"Recording Angle", "deg", kParamRecordingAngle, 0.0, 360.0, 0},
	    {"Meter L", "dB", kParamMeterLeftDb, kMinLevelDb, kMaxLevelDb, 1},
	    {"Meter R", "dB", kParamMeterRightDb, kMinLevelDb, kMaxLevelDb, 1},
	    // What the display needs. It used to read the angle and distance edit
	    // fields instead, but a field that is being typed into keeps the value
	    // it had, so the display was told the wrong thing while the parameter
	    // was already right. Every other readout travels this way.
	    {"Spot Azimuth", "deg", kParamSpotAzimuth, kMinSpotAngleDegrees, kMaxSpotAngleDegrees, 1},
	    {"Spot Distance", "m", kParamSpotDistance, 0.0, kMaxDistanceMeters, 3},

	};

	for (const auto& readout : readouts)
	{
		ParameterInfo info {};
		// assign takes ASCII and stores UTF-16; STR16 only works on a literal
		UString (info.title, str16BufferSize (String128)).assign (readout.title);
		UString (info.units, str16BufferSize (String128)).assign (readout.units);
		info.flags = ParameterInfo::kIsReadOnly;
		info.stepCount = kStepCountContinuous;
		info.id = static_cast<Vst::ParamID> (readout.id);
		info.unitId = kRootUnitId;

		// ParamRef appends the unit, so a label shows "23.595 ms" rather than a bare number
		auto* param = new ParamRef (info, readout.minPlain, readout.maxPlain);
		param->setPrecision (readout.precision);
		param->getInfo ().defaultNormalizedValue =
		    toNormalizedValue (readout.id, readout.id == kParamLevelLeftDb || readout.id == kParamLevelRightDb
		                                     ? kDefaultLevelDb
		                                     : readout.minPlain);
		param->setNormalized (param->getInfo ().defaultNormalizedValue);
		container.addParameter (param);
	}
}

//------------------------------------------------------------------------
// SpotAlignerController Implementation
//------------------------------------------------------------------------
tresult PLUGIN_API SpotAlignerController::initialize (FUnknown* context)
{
	// Here the Plug-in will be instantiated

	//---do not forget to call parent ------
	tresult result = EditControllerEx1::initialize (context);
	if (result != kResultOk)
	{
		return result;
	}

	//--- register the parameters of this plug-in ------------------
	addParameters ();

	//--- make the read only readouts show a useful value right away
	updateComputedReadouts ();

	return result;
}

//------------------------------------------------------------------------
void SpotAlignerController::addParameters ()
{
	parameters.init (kParamCount, 16);
	createParameters (parameters);
}

//------------------------------------------------------------------------
tresult PLUGIN_API SpotAlignerController::terminate ()
{
	// Here the Plug-in will be de-instantiated, last possibility to remove some memory!

	//---do not forget to call parent ------
	return EditControllerEx1::terminate ();
}

//------------------------------------------------------------------------
tresult PLUGIN_API SpotAlignerController::setComponentState (IBStream* state)
{
	// Here you get the state of the component (Processor part)
	if (!state)
		return kResultFalse;

	IBStreamer streamer (state, kLittleEndian);

	uint32 version = 0;
	if (!streamer.readInt32u (version))
		return kResultFalse;

	// a stream of an older version carries fewer parameters; the ones it does
	// not know keep their defaults
	const int32 stored = storedParamCount (version);
	if (stored < 0)
		return kResultFalse;

	const double* defaults = defaultNormalizedValues ();

	for (int32 i = 0; i < kParamCount; ++i)
	{
		double value = defaults[i];
		if (i < stored && !streamer.readDouble (value))
			return kResultFalse;

		setParamNormalized (static_cast<Vst::ParamID> (i), value);
	}

	return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API SpotAlignerController::setParamNormalized (Vst::ParamID tag, Vst::ParamValue value)
{
	const tresult result = EditControllerEx1::setParamNormalized (tag, value);

	// Choosing a technique loads the values it is defined with. They are set
	// through the host (see applyFormatPreset), so this arrives here once more
	// with those values, not with the format, and does not loop.
	if (tag == kParamMainFormat)
		applyFormatPreset (*this, static_cast<int> (toPlainValue (kParamMainFormat, value) + 0.5));



	// The readouts have to follow every input they are made of. Listing those
	// inputs by hand is what lost the two height parameters when they were
	// added: the delay stopped following them. So everything recalculates
	// except the readouts themselves, which would only recurse.
	if (tag != kParamComputedSpeed && tag != kParamComputedDelay)
		updateComputedReadouts ();

	return result;
}

//------------------------------------------------------------------------
double SpotAlignerController::computeSpeedOfSound ()
{
	double values[kParamCount];
	for (int32 i = 0; i < kParamCount; ++i)
		values[i] = getParamNormalized (static_cast<Vst::ParamID> (i));

	return effectiveSpeedOfSound (values);
}

//------------------------------------------------------------------------
void SpotAlignerController::updateComputedReadouts ()
{
	double values[kParamCount];
	for (int32 i = 0; i < kParamCount; ++i)
		values[i] = getParamNormalized (static_cast<Vst::ParamID> (i));

	//--- speed of sound that is in use ---------------------------------
	EditControllerEx1::setParamNormalized (kParamComputedSpeed,
	                                       toNormalizedValue (kParamComputedSpeed, effectiveSpeedOfSound (values)));

	//--- delay that is applied to the object ---------------------------
	EditControllerEx1::setParamNormalized (kParamComputedDelay,
	                                       toNormalizedValue (kParamComputedDelay, effectiveDelayMs (values)));

	//--- what the placement comes out as -------------------------------
	// Computed here rather than in the editor, because this is where the
	// parameter values really live: a view that reads them back from the
	// controls it finds in the description got the wrong answer for every
	// control that sits nested inside another container, which is what made
	// the two delays of a side come out at the middle distance instead of at
	// their own. The numbers are handed to the editor the way every other
	// readout is, as parameters the host forwards.
	const double speed = usableSpeedOfSound (values);
	const int format = static_cast<int> (toPlainValue (kParamMainFormat, values[kParamMainFormat]) + 0.5);
	const Panorama::ArrayGeometry array = arrayGeometryFromParameters (values);
	const Panorama::ObjectPlacement object = objectPlacementFromParameters (values);
	const Panorama::StereoSetup setup = Panorama::stereoSetup (array, object, speed);

	const double angle = recordingAngleDegreesFromParameters (values);


	setPlain (kParamDelayLeftMs, setup.left.delaySeconds * 1000.0);
	setPlain (kParamDelayRightMs, setup.right.delaySeconds * 1000.0);
	setPlain (kParamLevelLeftDb, gainToDb (setup.left.gain));
	setPlain (kParamLevelRightDb, gainToDb (setup.right.gain));

	// Saying so when the array was changed by hand. The marker goes away by
	// itself as soon as the values are the technique's own again, because this
	// is asked of the geometry and not remembered from the last edit.
	const bool edited = Panorama::formatGeometryIsEdited (format, array);
	setPlain (kParamGeometryNote, edited ? 1.0 : 0.0);

	// Temporary: what the preset and the marker are looking at. Remove with the
	// rest of the diagnostics.
	if (const char* temp = std::getenv ("TEMP"))
	{
		std::string path (temp);
		path += "\\spotaligner_array.log";
		if (FILE* file = std::fopen (path.c_str (), "a"))
		{
			std::fprintf (file,
			              "format=%d spacingCm=%.1f aimedAt=%.1f patternA=%.2f | standard %.1f / %.1f / %.2f | "
			              "edited=%d\n",
			              format, array.spacingMeters * 100.0, array.halfAngleDegrees, array.patternA,
			              Panorama::formatStandardSpacingCm (format), Panorama::formatHalfAngleDegrees (format),
			              Panorama::formatPatternA (format), edited ? 1 : 0);
			std::fclose (file);
		}
	}

	// A recording angle of the whole front means the array never reaches the
	// extremes; it is reported as it is, the editor words it.
	setPlain (kParamRecordingAngle, angle);

	// the object's place in the image, for the display only
	setPlain (kParamSpotAzimuth, object.azimuthDegrees);
	setPlain (kParamSpotDistance, object.distanceMeters);

}

//------------------------------------------------------------------------
void SpotAlignerController::setPlain (Vst::ParamID id, double plainValue)
{
	EditControllerEx1::setParamNormalized (id, toNormalizedValue (static_cast<int32> (id), plainValue));
}

//------------------------------------------------------------------------
double SpotAlignerController::gainToDb (double gain)
{
	if (gain <= 1.0e-6)
		return kMinLevelDb;

	const double db = 20.0 * std::log10 (gain);
	return db < kMinLevelDb ? kMinLevelDb : (db > kMaxLevelDb ? kMaxLevelDb : db);
}

//------------------------------------------------------------------------
tresult PLUGIN_API SpotAlignerController::setState (IBStream* state)
{
	// Here you get the state of the controller

	return kResultTrue;
}

//------------------------------------------------------------------------
tresult PLUGIN_API SpotAlignerController::getState (IBStream* state)
{
	// Here you are asked to deliver the state of the controller (if needed)
	// Note: the real state of your plug-in is saved in the processor

	return kResultTrue;
}

//------------------------------------------------------------------------
IPlugView* PLUGIN_API SpotAlignerController::createView (FIDString name)
{
	// Here the Host wants to open your editor (if you have one)
	if (FIDStringsEqual (name, Vst::ViewType::kEditor))
	{
		// create your editor here and return a IPlugView ptr of it
		auto* view = new VSTGUI::VST3Editor (this, "view", "editor.uidesc");
		return view;
	}
	return nullptr;
}

//------------------------------------------------------------------------
} // namespace MyCompanyName
