//------------------------------------------------------------------------
// Copyright(c) 2026 Forrest's Plug-ins.
//------------------------------------------------------------------------
#pragma once

#include "parameters.h"

#include "public.sdk/source/vst/vsteditcontroller.h"

namespace MyCompanyName {

/** Adds the parameters that describe the array itself, next to the note about
	the object microphone's pattern. */
void addArrayParameters (Steinberg::Vst::ParameterContainer& container);

/** Fills in the spacing, the angle and the pattern the named technique is
	defined with, and tells the host, so that the processor follows. */
void applyFormatPreset (Steinberg::Vst::EditController& controller, int format);




} // namespace MyCompanyName
