//------------------------------------------------------------------------
// Copyright(c) 2026 Forrest's Plug-ins.
//------------------------------------------------------------------------

#pragma once

#include "public.sdk/source/vst/vstaudioeffect.h"

#include "formatRules.h"
#include "parameters.h"
#include "spotPanner.h"

#include <vector>

namespace MyCompanyName {

//------------------------------------------------------------------------
/** Single channel delay line with fractional (sub sample) read access.

	The distance to the main microphone almost never lands exactly on a
	sample boundary, so the delayed signal has to be interpolated between two
	samples. A fractional read position also keeps the output smooth while
	the distance is being moved. */
class DelayLine
{
public:
	/** Allocates room for at least @a maxDelaySamples samples of delay and
	    clears the line. */
	void prepare (double sampleRate, double maxDelaySamples);

	/** Sets the delay for the following block, in samples. Call this once per
	    block, then processSample() for every sample. */
	void setDelay (double delayInSamples);

	/** Writes one input sample into the line and returns the delayed one. */
	float processSample (float input);

	void clear ();

	bool isPrepared () const { return !buffer.empty (); }

	/** Largest delay this line can produce in samples. */
	double maxDelayInSamples () const { return static_cast<double> (buffer.size ()) - 2.0; }

private:
	/** Reads the (fractional) sample @a readPosition back in time. */
	float readAt (double readPosition) const;

	std::vector<float> buffer;
	Steinberg::int64 writeIndex = 0;

	/** Distance between the write position and the read position, in samples. */
	double delay = 0.0;
};

//------------------------------------------------------------------------
//  SpotAlignerProcessor
//------------------------------------------------------------------------
class SpotAlignerProcessor : public Steinberg::Vst::AudioEffect
{
public:
	SpotAlignerProcessor ();
	~SpotAlignerProcessor () SMTG_OVERRIDE;

    // Create function
	static Steinberg::FUnknown* createInstance (void* /*context*/) 
	{ 
		return (Steinberg::Vst::IAudioProcessor*)new SpotAlignerProcessor; 
	}

	//--- ---------------------------------------------------------------------
	// AudioEffect overrides:
	//--- ---------------------------------------------------------------------
	/** Called at first after constructor */
	Steinberg::tresult PLUGIN_API initialize (Steinberg::FUnknown* context) SMTG_OVERRIDE;
	
	/** Called at the end before destructor */
	Steinberg::tresult PLUGIN_API terminate () SMTG_OVERRIDE;
	
	/** Switch the Plug-in on/off */
	Steinberg::tresult PLUGIN_API setActive (Steinberg::TBool state) SMTG_OVERRIDE;

	/** Will be called before any process call */
	Steinberg::tresult PLUGIN_API setupProcessing (Steinberg::Vst::ProcessSetup& newSetup) SMTG_OVERRIDE;
	
	/** Asks if a given sample size is supported see SymbolicSampleSizes. */
	Steinberg::tresult PLUGIN_API canProcessSampleSize (Steinberg::int32 symbolicSampleSize) SMTG_OVERRIDE;

	/** The host proposes the bus arrangements it can drive. */
	Steinberg::tresult PLUGIN_API setBusArrangements (Steinberg::Vst::SpeakerArrangement* inputs,
	                                                  Steinberg::int32 numIns,
	                                                  Steinberg::Vst::SpeakerArrangement* outputs,
	                                                  Steinberg::int32 numOuts) SMTG_OVERRIDE;

	/** Here we go...the process call */
	Steinberg::tresult PLUGIN_API process (Steinberg::Vst::ProcessData& data) SMTG_OVERRIDE;
		
	/** For persistence */
	Steinberg::tresult PLUGIN_API setState (Steinberg::IBStream* state) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API getState (Steinberg::IBStream* state) SMTG_OVERRIDE;

//------------------------------------------------------------------------
protected:
	/** Reads the parameter changes of this block into parameterValues. */
	void applyParameterChanges (Steinberg::Vst::IParameterChanges* changes);

	/** The array and the object as the DSP needs them: geometry of the main
	    pair from the selected format, position of the object from the
	    parameters. */
	Panorama::ArrayGeometry currentArray () const;
	Panorama::ObjectPlacement currentObject () const;

	/** Delay and gain of the two sides, for the current parameters. */
	Panorama::StereoSetup currentStereoSetup () const;

	/** One pole coefficient that reaches a new value over about the given
	    number of seconds, for the smoothing of delays and gains. */
	double smoothingCoefficient (double seconds) const;

	/** Keeps a delay inside what a line can hold. */
	double clampToLine (const DelayLine& line, double delayInSamples) const;

	/** How many output channels the host really handed over this block, counted
	    only where a buffer stands behind the channel. This is the whole of what
	    a plug-in can know about its track, and the same question in every host.
	    Checked on every block, so a track that becomes stereo is picked up
	    immediately. */
	Steinberg::int32 usableOutputChannels (const Steinberg::Vst::ProcessData& data);

	/** Tells the editor, through the parameter the host forwards, how many
	    output channels the plug-in has to work with. */
	void reportTrackChannels (Steinberg::Vst::ProcessData& data, Steinberg::int32 channels);

	/** Tells the editor what each side is putting out right now, so that the
	    meters can show it next to the level the placement asks for. */
	void reportMeters (Steinberg::Vst::ProcessData& data, double leftDb, double rightDb);

	/** Current normalized parameter values, indexed by SpotAlignerParams. */
	double parameterValues[kParamCount];

	/** One line per microphone: the two outer ones of the array stand apart and
	    a tree has a third one in front of them, so the object reaches each of
	    them at its own time. */
	DelayLine delayLeft;
	DelayLine delayRight;
	DelayLine delayCentre;

	/** Gains of the sides, smoothed so that moving the object does not step the
	    level. The centre gain is what one side receives. */
	double gainLeft = 0.70710678118654752;
	double gainRight = 0.70710678118654752;
	double gainCentre = 0.0;

	/** Delays of the microphones in samples, smoothed for the same reason. */
	double delayLeftSamples = 0.0;
	double delayRightSamples = 0.0;
	double delayCentreSamples = 0.0;

	/** False until the placement has been applied once: the first block of a
	    run starts at its value instead of fading into it. */
	bool placementStarted = false;

	/** Highest sample of the block being written, one per side, cleared after
	    each block and handed to the meters. */
	double peakLeft = 0.0;
	double peakRight = 0.0;

	/** What the meters were last told, and the release that keeps them readable. */
	double meterLeftDb = kMinLevelDb;
	double meterRightDb = kMinLevelDb;

	/** Sample rate announced by the host in setupProcessing. */
	double currentSampleRate = 44100.0;

	/** Samples of the block being processed, for the smoothing. */
	Steinberg::int32 blockSamples = 0;

	/** Temporary test hook, see forceMonoTrackForTesting in processor.cpp. */
	bool forceMonoTrack = false;
	Steinberg::int64 samplesSinceTestSwitchCheck = 0;
	Steinberg::int32 lastLoggedChannels = -2;

	/** What was last reported to the host, -1 before anything was. The local
	    default value is deliberately not used for this. */
	Steinberg::int32 lastReportedChannels = -1;
};

//------------------------------------------------------------------------
} // namespace MyCompanyName
