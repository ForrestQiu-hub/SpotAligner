//------------------------------------------------------------------------
// Copyright(c) 2026 Forrest's Plug-ins.
//------------------------------------------------------------------------
// The array as itself, and the note about the object microphone.
//
// A format used to be a straight jacket: the spacing, the angle the two
// microphones are aimed at and their polar pattern were all looked up from its
// name, so a pair that is spaced like an ORTF but aimed differently - which is
// a real technique, and one the published angle of an ORTF does not describe -
// could not be set at all. Now the format is a preset that fills the three
// values in, and they can be changed afterwards.
//
// The second part is the note: what the object microphone's pattern means for
// the alignment. It is a note and not a filter on purpose - see the analysis:
// the pattern of a microphone is already in the recording of that microphone,
// so putting it in again would count it twice. What it does decide is how much
// of the spot is direct sound, and that is what decides how much the alignment
// is worth and how much comb filtering it brings with it.
//------------------------------------------------------------------------

#include "arrayParameters.h"

#include "public.sdk/source/vst/vstparameters.h"
#include "pluginterfaces/base/ustring.h"

#include <cstdio>
#include <cstring>

namespace MyCompanyName {

using namespace Steinberg;
using namespace Steinberg::Vst;

//------------------------------------------------------------------------
namespace {

/** Gives a list parameter its default entry. */
void setListDefault (StringListParameter& parameter, int index, int count)
{
	// A list parameter is stepped: entry 0 is 0 and the last entry is 1, so the
	// quotient is over one less than the count. Using the count itself put the
	// default one entry out, which showed a figure of eight where the technique
	// is done with cardioids.
	parameter.getInfo ().stepCount = static_cast<int32> (count - 1);
	parameter.getInfo ().defaultNormalizedValue =
	    count > 1 ? static_cast<ParamValue> (index) / static_cast<ParamValue> (count - 1) : 0.0;
	parameter.setNormalized (parameter.getInfo ().defaultNormalizedValue);
}

/** A range parameter that appends its unit to the value it shows. */
class UnitRef : public RangeParameter
{
public:
	UnitRef (const ParameterInfo& info, ParamValue minPlain, ParamValue maxPlain)
	: RangeParameter (info, minPlain, maxPlain)
	{
	}

	void toString (ParamValue valueNormalized, String128 string) const SMTG_OVERRIDE
	{
		// The value that arrives here is the normalized one. RangeParameter
		// turns it into the plain value it shows, and that is what has to be
		// printed: formatting it here printed 0.6 for 55 degrees of a 0 to 90
		// degree range, which is exactly what that quotient is.
		RangeParameter::toString (valueNormalized, string);

		if (units[0] == 0)
			return;

		UString text (string, str16BufferSize (String128));
		text.append (STR16 (" "));
		text.append (units);
	}

	String128 units {};
};


/** Sets a parameter on the controller and tells the host about it, so that the
    processor is given the same value. */
void setPlainParameter (EditController& controller, int32 id, double plainValue)
{
	const ParamValue normalized = toNormalizedValue (id, plainValue);
	controller.setParamNormalized (static_cast<ParamID> (id), normalized);
	controller.performEdit (static_cast<ParamID> (id), normalized);
}

/** A parameter that shows a note instead of its value. Its value carries the
    entry it is about, so the text is worked out when it is asked for. */
class NoteRef : public RangeParameter
{
public:
	NoteRef (const ParameterInfo& info, ParamValue minPlain, ParamValue maxPlain)
	: RangeParameter (info, minPlain, maxPlain)
	{
	}

	/** What is shown for each entry of the range. */
	const char* notes[8] = {};

	void toString (ParamValue value, String128 string) const SMTG_OVERRIDE
	{
		// the value is normalized; the note is about the entry it stands for
		int index = static_cast<int> (toPlain (value) + 0.5);
		if (index < 0 || index > 7 || !notes[index])
			index = 0;

		UString (string, str16BufferSize (String128)).assign (notes[index] ? notes[index] : "");
	}
};

} // namespace

//------------------------------------------------------------------------
void addArrayParameters (ParameterContainer& container)
{
	{
		ParameterInfo info {};
		UString (info.title, str16BufferSize (String128)).assign ("Array Half Angle");
		info.flags = ParameterInfo::kCanAutomate;
		info.stepCount = kStepCountContinuous;
		info.id = static_cast<ParamID> (kParamMainHalfAngle);
		info.unitId = kRootUnitId;

		auto* angle = new UnitRef (info, kMinMainHalfAngleDegrees, kMaxMainHalfAngleDegrees);
		UString (angle->units, str16BufferSize (String128)).assign ("deg");
		angle->setPrecision (1);

		const double wanted = Panorama::formatHalfAngleDegrees (Panorama::kFormatAB);
		angle->getInfo ().defaultNormalizedValue = toNormalizedValue (kParamMainHalfAngle, wanted);
		angle->setNormalized (angle->getInfo ().defaultNormalizedValue);
		container.addParameter (angle);
	}

	{
		auto* pattern = new StringListParameter (STR16 ("Array Pattern"), kParamMainPattern, nullptr,
		                                         ParameterInfo::kCanAutomate | ParameterInfo::kIsList);
		for (int i = 0; i < Panorama::kPatternCount; ++i)
		{
			String128 name {};
			UString (name, str16BufferSize (String128)).assign (Panorama::patternName (i));
			pattern->appendString (name);
		}

		setListDefault (*pattern, Panorama::patternListIndexForA (Panorama::formatPatternA (Panorama::kFormatAB)),
		                Panorama::kPatternCount);
		container.addParameter (pattern);
	}

	// Whether the spot is given a left/right time difference. On, the two
	// channels are delayed by the time the sound takes to reach their own
	// microphone, so the object is placed by the geometry. Off, both channels
	// get one and the same delay - the one of the microphone on the side the
	// object is nearer, and the left one when it sits on the centre line.
	{
		auto* difference = new StringListParameter (STR16 ("L/R Time Difference"),
		                                            kParamDelayDifference, nullptr,
		                                            ParameterInfo::kCanAutomate | ParameterInfo::kIsList);
		difference->appendString (STR16 ("Off"));		difference->appendString (STR16 ("On"));
		setListDefault (*difference, 1, 2);
		container.addParameter (difference);
	}

	// Saying that the array is no longer the technique it is named after. Two
	// words and nothing else: it sits next to the format field, and it goes
	// away by itself when the values are the technique's own again.
	{
		ParameterInfo info {};
		UString (info.title, str16BufferSize (String128)).assign ("Array Edited");
		info.flags = ParameterInfo::kIsReadOnly;
		info.stepCount = kStepCountContinuous;
		info.id = static_cast<ParamID> (kParamGeometryNote);
		info.unitId = kRootUnitId;

		auto* edited = new NoteRef (info, 0.0, 1.0);
		edited->setPrecision (0);
		edited->notes[0] = "";
		edited->notes[1] = "edited";
		container.addParameter (edited);
	}
}

//------------------------------------------------------------------------
void applyFormatPreset (EditController& controller, int format)
{
	// A technique is defined by its spacing too, but only the ones that have
	// one: a tree and a plain pair are whatever the user spaced them at, and a
	// coincident pair has no spacing at all.
	const double spacingCm = Panorama::formatPrescribedSpacingCm (format);
	if (spacingCm > 0.0)
		setPlainParameter (controller, kParamAbSpacing, spacingCm);

	setPlainParameter (controller, kParamMainHalfAngle, Panorama::formatHalfAngleDegrees (format));
	setPlainParameter (controller, kParamMainPattern,
	                   Panorama::patternListIndexForA (Panorama::formatPatternA (format)));
}

//------------------------------------------------------------------------


//------------------------------------------------------------------------
} // namespace MyCompanyName
