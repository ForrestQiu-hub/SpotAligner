//------------------------------------------------------------------------
// Copyright(c) 2026 Forrest's Plug-ins.
//------------------------------------------------------------------------

#pragma once

#include "public.sdk/source/vst/vsteditcontroller.h"

namespace MyCompanyName {

//------------------------------------------------------------------------
//  SpotAlignerController
//------------------------------------------------------------------------
class SpotAlignerController : public Steinberg::Vst::EditControllerEx1
{
public:
//------------------------------------------------------------------------
	SpotAlignerController () = default;
	~SpotAlignerController () SMTG_OVERRIDE = default;

    // Create function
	static Steinberg::FUnknown* createInstance (void* /*context*/)
	{
		return (Steinberg::Vst::IEditController*)new SpotAlignerController;
	}

	//--- from IPluginBase -----------------------------------------------
	Steinberg::tresult PLUGIN_API initialize (Steinberg::FUnknown* context) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API terminate () SMTG_OVERRIDE;

	//--- from EditController --------------------------------------------
	Steinberg::tresult PLUGIN_API setComponentState (Steinberg::IBStream* state) SMTG_OVERRIDE;
	Steinberg::IPlugView* PLUGIN_API createView (Steinberg::FIDString name) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API setParamNormalized (Steinberg::Vst::ParamID tag,
	                                                  Steinberg::Vst::ParamValue value) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API setState (Steinberg::IBStream* state) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API getState (Steinberg::IBStream* state) SMTG_OVERRIDE;

 	//---Interface---------
	DEFINE_INTERFACES
		// Here you can add more supported VST3 interfaces
		// DEF_INTERFACE (Vst::IXXX)
	END_DEFINE_INTERFACES (EditController)
    DELEGATE_REFCOUNT (EditController)

//------------------------------------------------------------------------
protected:
	/** Creates and registers the parameter objects of this plug-in. It has to
	    run inside the controller because the parameter container is a
	    protected member of EditController. */
	void addParameters ();

	/** Recomputes every read only readout from the current parameters and
	    publishes it: the speed of sound in use, the delay in use, and the six
	    numbers next to the display (both delays, both levels, the recording
	    angle and where the object lands in the image). */
	void updateComputedReadouts ();

	/** Publishes a plain value on a read only parameter. */
	void setPlain (Steinberg::Vst::ParamID id, double plainValue);

	/** Speed of sound (m/s) the current parameters result in. */
	double computeSpeedOfSound ();

	/** A linear gain as dB, kept inside the range the readout reports in. */
	static double gainToDb (double gain);
};

//------------------------------------------------------------------------
} // namespace MyCompanyName
