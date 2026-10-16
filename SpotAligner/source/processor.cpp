//------------------------------------------------------------------------
// Copyright(c) 2026 Forrest's Plug-ins.
//------------------------------------------------------------------------

#include "processor.h"
#include "cids.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"

#include <algorithm>
#include <cmath>
#include <cstring>

using namespace Steinberg;

namespace MyCompanyName {

/** Level of a centre image: a mono object sent to both channels at equal
	power keeps the loudness it had on one channel. */
constexpr float kCentreGain = 0.70710678f;

/** Temporary: when this file exists, the track counts as mono, so that the
	warning can be checked in a host whose tracks are always stereo (REAPER
	tracks cannot be set to one channel). Remove together with the note in the
	test instructions. */
bool forceMonoTrackForTesting ()
{
	// no test hooks in a released build
	return false;
}

/** How often the test switch above is looked at, as a share of a second. Only
	the diagnostic needs this; the real check runs on every block. */
constexpr double kTestSwitchCheckSeconds = 0.25;

/** The name of the host, for the diagnostics below. Several hosts write into
	the same log file, so every line has to say where it came from. */
std::string hostNameFrom (Steinberg::FUnknown* context)
{
	std::string host;
	if (!context)
		return host;

	Steinberg::FUnknownPtr<Steinberg::Vst::IHostApplication> application (context);
	if (!application)
		return host;

	Steinberg::Vst::String128 name {};
	if (application->getName (name) != Steinberg::kResultOk)
		return host;

	// host names are plain ASCII in practice
	for (int i = 0; i < 96 && name[i] != 0; ++i)
	{
		if (name[i] < 128)
			host += static_cast<char> (name[i]);
	}
	return host;
}

/** Temporary: records what the host says about the buses, so that a wrong
	channel count can be traced from the outside instead of guessed at.
	Remove together with the test switch. */
void writeBusDiagnostics (const std::string& host, const char* what,
                          Steinberg::Vst::SpeakerArrangement arrangementIn,
                          Steinberg::Vst::SpeakerArrangement arrangementOut, Steinberg::int32 channels,
                          const Steinberg::Vst::ProcessData* data, bool forced)
{
		// no file logging in a released build
	}

//------------------------------------------------------------------------
// DelayLine
//------------------------------------------------------------------------
void DelayLine::prepare (double sampleRate, double maxDelaySamples)
{
	if (sampleRate <= 0.0 || maxDelaySamples <= 0.0)
	{
		buffer.clear ();
		writeIndex = 0;
		return;
	}

	// Two extra samples: one for the interpolation partner and one so that a
	// delay of exactly maxDelaySamples is still readable.
	const double needed = maxDelaySamples + 2.0;
	uint64 size = 2;
	while (static_cast<double> (size) < needed)
		size <<= 1;

	if (buffer.size () != static_cast<size_t> (size))
		buffer.assign (static_cast<size_t> (size), 0.0f);

	clear ();
}

//------------------------------------------------------------------------
void DelayLine::clear ()
{
	std::fill (buffer.begin (), buffer.end (), 0.0f);
	writeIndex = 0;
}

//------------------------------------------------------------------------
void DelayLine::setDelay (double delayInSamples)
{
	const double maxDelay = maxDelayInSamples ();
	if (delayInSamples < 0.0)
		delayInSamples = 0.0;
	else if (delayInSamples > maxDelay)
		delayInSamples = maxDelay;

	delay = delayInSamples;
}

//------------------------------------------------------------------------
float DelayLine::readAt (double readPosition) const
{
	const int64 size = static_cast<int64> (buffer.size ());
	if (size <= 0)
		return 0.0f;

	// wrap into [0, size)
	readPosition = std::fmod (readPosition, static_cast<double> (size));
	if (readPosition < 0.0)
		readPosition += static_cast<double> (size);

	int64 index = static_cast<int64> (readPosition);
	if (index >= size)
		index -= size;
	else if (index < 0)
		index += size;

	const int64 next = (index + 1) % size;
	const float fraction = static_cast<float> (readPosition - static_cast<double> (index));

	return buffer[static_cast<size_t> (index)] * (1.0f - fraction) + buffer[static_cast<size_t> (next)] * fraction;
}

//------------------------------------------------------------------------
float DelayLine::processSample (float input)
{
	if (buffer.empty ())
		return input;

	buffer[static_cast<size_t> (writeIndex)] = input;

	// read before the write position advances, so a delay of zero samples
	// returns the sample that was just written
	const float delayed = readAt (static_cast<double> (writeIndex) - delay);

	++writeIndex;
	if (writeIndex >= static_cast<int64> (buffer.size ()))
		writeIndex = 0;

	return delayed;
}

//------------------------------------------------------------------------
// SpotAlignerProcessor
//------------------------------------------------------------------------
SpotAlignerProcessor::SpotAlignerProcessor ()
{
	//--- set the wanted controller for our processor
	setControllerClass (kSpotAlignerControllerUID);

	//--- start from the documented default values
	const double* defaults = defaultNormalizedValues ();
	for (int32 i = 0; i < kParamCount; ++i)
		parameterValues[i] = defaults[i];
}

//------------------------------------------------------------------------
SpotAlignerProcessor::~SpotAlignerProcessor ()
{}

//------------------------------------------------------------------------
tresult PLUGIN_API SpotAlignerProcessor::initialize (FUnknown* context)
{
	// Here the Plug-in will be instantiated
	
	//---always initialize the parent-------
	tresult result = AudioEffect::initialize (context);
	// if everything Ok, continue
	if (result != kResultOk)
	{
		return result;
	}

	//--- create Audio IO ------
	// The object is a single microphone, the result is a stereo image of it
	// standing in the main pair. The input is taken as stereo so that the
	// plug-in drops onto any track; the two channels are summed, because one
	// object is one signal.
	addAudioInput (STR16 ("Spot In (L+R)"), Steinberg::Vst::SpeakerArr::kStereo);
	addAudioOutput (STR16 ("Spot Out"), Steinberg::Vst::SpeakerArr::kStereo);

	return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API SpotAlignerProcessor::terminate ()
{
	// Here the Plug-in will be de-instantiated, last possibility to remove some memory!
	
	//---do not forget to call parent ------
	return AudioEffect::terminate ();
}

//------------------------------------------------------------------------
tresult PLUGIN_API SpotAlignerProcessor::setActive (TBool state)
{
	if (state)
	{
		// never play back samples from a previous run, and start the placement
		// at its value instead of fading into it
		delayLeft.clear ();
		delayRight.clear ();
		delayCentre.clear ();
		placementStarted = false;

		// the test switch is read on the first block of the new run
		samplesSinceTestSwitchCheck = 0;
		forceMonoTrack = forceMonoTrackForTesting ();
	}

	return AudioEffect::setActive (state);
}

//------------------------------------------------------------------------
tresult PLUGIN_API SpotAlignerProcessor::process (Vst::ProcessData& data)
{
	//--- First: read input parameter changes ------------------------
	applyParameterChanges (data.inputParameterChanges);

	const int32 numSamples = data.numSamples;
	if (numSamples <= 0)
		return kResultOk;

	//--- can the track carry the stereo image? ----------------------
	// This runs on every block, so a track that gains a second output channel
	// is processed normally again from that very block on: there is no reload,
	// no reset and nothing to switch by hand. The delay line keeps being fed
	// while the object passes through, so the moment it opens there are no
	// stale samples waiting in it.
	const int32 outputChannels = usableOutputChannels (data);
	const bool stereo = outputChannels >= static_cast<int32> (kOutputChannelsForStereo);
	reportTrackChannels (data, outputChannels);

	const Vst::Sample32* input = nullptr;
	if (data.numInputs > 0 && data.inputs[0].numChannels > 0)
		input = data.inputs[0].channelBuffers32[0];

	const Vst::Sample32* inputRight = nullptr;
	if (data.numInputs > 0 && data.inputs[0].numChannels > 1)
		inputRight = data.inputs[0].channelBuffers32[1];

	Vst::Sample32* outputLeft = nullptr;
	Vst::Sample32* outputRight = nullptr;
	if (data.numOutputs > 0)
	{
		if (data.outputs[0].numChannels > 0)
			outputLeft = data.outputs[0].channelBuffers32[0];
		if (data.outputs[0].numChannels > 1)
			outputRight = data.outputs[0].channelBuffers32[1];
	}

	if (!input || !delayLeft.isPrepared () || !delayRight.isPrepared () || !delayCentre.isPrepared ())
	{
		// nothing to delay: report the outputs as silent
		for (int32 bus = 0; bus < data.numOutputs; ++bus)
		{
			auto& out = data.outputs[bus];
			for (int32 channel = 0; channel < out.numChannels; ++channel)
			{
				if (out.channelBuffers32 && out.channelBuffers32[channel])
					std::memset (out.channelBuffers32[channel], 0,
					             static_cast<size_t> (numSamples) * sizeof (Vst::Sample32));
			}
			out.silenceFlags = (static_cast<uint64> (1) << out.numChannels) - 1;
		}
		return kResultOk;
	}

	//--- where the object stands in the main pair -------------------
	// Each side is delayed by the time the object's sound takes to reach that
	// microphone of the array, and weighted by how far off that microphone's
	// axis the object is. A coincident array (XY, MS) comes out as plain
	// amplitude panning, a spaced one (AB, ORTF, NOS, DIN) adds the time
	// difference the two microphones really have.
	const Panorama::StereoSetup wanted = currentStereoSetup ();
	Panorama::StereoSetup setup = wanted;
	const bool bypassed = parameterValues[kParamBypass] >= 0.5;

	// a track that cannot carry the image, or a bypassed plug-in, lets the
	// object through as it is: one microphone, centred, at equal power
	const bool passThrough = bypassed || !stereo;

	// Delays and gains are moved towards their new values instead of jumping:
	// a step in either is a click, and while the object is being moved a glide
	// is what a microphone being moved would sound like anyway. The first block
	// after starting or loading starts at the value instead of fading into it,
	// so that opening the plug-in does not fade the placement in.
	if (!placementStarted)
	{
		placementStarted = true;
		gainLeft = setup.left.gain;
		gainRight = setup.right.gain;
		gainCentre = setup.centre.gain;
	}

	const double delaySmoothing = smoothingCoefficient (0.05);
	const double gainSmoothing = smoothingCoefficient (0.005);

	const double wantedLeft = clampToLine (delayLeft, setup.left.delaySeconds * currentSampleRate);
	const double wantedRight = clampToLine (delayRight, setup.right.delaySeconds * currentSampleRate);
	const double wantedCentre = clampToLine (delayCentre, setup.centre.delaySeconds * currentSampleRate);

	if (passThrough)
	{
		// centred, so that the level matches what the placed object has
		setup.left.gain = 0.70710678118654752;
		setup.right.gain = 0.70710678118654752;
		setup.centre.gain = 0.0;

		// the lines keep running so that leaving bypass, or a track that becomes
		// stereo, does not start from stale samples
		delayLeftSamples = wantedLeft;
		delayRightSamples = wantedRight;
		delayCentreSamples = wantedCentre;
	}

	delayLeft.setDelay (delayLeftSamples);
	delayRight.setDelay (delayRightSamples);
	delayCentre.setDelay (delayCentreSamples);

	for (int32 i = 0; i < numSamples; ++i)
	{
		// one object is one signal: the two input channels are summed
		const float mono = inputRight ? 0.5f * (input[i] + inputRight[i]) : input[i];

		// the lines are fed in both cases, so the object is already in them when
		// the placement opens
		const float fromLeft = delayLeft.processSample (mono);
		const float fromRight = delayRight.processSample (mono);
		const float fromCentre = delayCentre.processSample (mono);

		gainLeft += (setup.left.gain - gainLeft) * gainSmoothing;
		gainRight += (setup.right.gain - gainRight) * gainSmoothing;
		gainCentre += (setup.centre.gain - gainCentre) * gainSmoothing;

		// the centre microphone of a tree goes to both sides equally
		const float valueLeft =
		    static_cast<float> ((passThrough ? mono : fromLeft) * gainLeft + fromCentre * gainCentre);
		const float valueRight =
		    static_cast<float> ((passThrough ? mono : fromRight) * gainRight + fromCentre * gainCentre);

		// A host may leave the output buffers out for a block whose output it
		// does not need. The lines keep running and the samples are dropped.
		if (outputLeft)
			outputLeft[i] = valueLeft;
		if (outputRight)
			outputRight[i] = valueRight;

	}


	// the smoothed delays follow the wanted ones between the blocks
	delayLeftSamples += (wantedLeft - delayLeftSamples) * delaySmoothing;
	delayRightSamples += (wantedRight - delayRightSamples) * delaySmoothing;
	delayCentreSamples += (wantedCentre - delayCentreSamples) * delaySmoothing;

	if (outputLeft || outputRight)
		data.outputs[0].silenceFlags = 0;

	return kResultOk;
}

//------------------------------------------------------------------------
Panorama::ArrayGeometry SpotAlignerProcessor::currentArray () const
{
	// one place turns parameters into geometry, shared with the editor's
	// readouts, see arrayGeometryFromParameters in parameters.h
	return arrayGeometryFromParameters (parameterValues);
}

//------------------------------------------------------------------------
Panorama::ObjectPlacement SpotAlignerProcessor::currentObject () const
{
	return objectPlacementFromParameters (parameterValues);
}

//------------------------------------------------------------------------
Panorama::StereoSetup SpotAlignerProcessor::currentStereoSetup () const
{
	// The switch is a list: 0 = Off, one delay for the whole object (the traditional
	// way); 1 = On, the two channels are delayed to their own microphone.
	const bool channelTimeDifference = parameterValues[kParamDelayDifference] > 0.5;

	return Panorama::stereoSetup (currentArray (), currentObject (), usableSpeedOfSound (parameterValues),
	                              channelTimeDifference);
}

//------------------------------------------------------------------------
double SpotAlignerProcessor::smoothingCoefficient (double seconds) const
{
	// one pole towards the new value, reached to within a thousandth after
	// about seven of these time constants
	const double time = seconds * currentSampleRate;
	if (time <= 0.0)
		return 1.0;

	const double block = blockSamples > 0 ? static_cast<double> (blockSamples) : 1.0;
	return 1.0 - std::exp (-block / time);
}

//------------------------------------------------------------------------
double SpotAlignerProcessor::clampToLine (const DelayLine& line, double delayInSamples) const
{
	if (delayInSamples < 0.0)
		return 0.0;

	const double maximum = line.maxDelayInSamples ();
	return delayInSamples > maximum ? maximum : delayInSamples;
}

//------------------------------------------------------------------------
int32 SpotAlignerProcessor::usableOutputChannels (const Vst::ProcessData& data)
{
	//--- temporary test switch -------------------------------------
	// Looked at now and then instead of once per activation, so the warning can
	// be watched appearing and disappearing without reloading the plug-in. A
	// file read in the audio loop is acceptable for a diagnostic and would not
	// belong in shipping code.
	samplesSinceTestSwitchCheck += data.numSamples;
	const double checkInterval = kTestSwitchCheckSeconds * currentSampleRate;

	if (checkInterval <= 0.0 || static_cast<double> (samplesSinceTestSwitchCheck) >= checkInterval)
	{
		samplesSinceTestSwitchCheck = 0;
		forceMonoTrack = forceMonoTrackForTesting ();
	}

	if (forceMonoTrack)
		return 1;

	//--- the real question -----------------------------------------
	// How many output channels the track can carry, taken from the arrangement
	// the host committed to. The buffers of a single block cannot answer this:
	// a host is allowed to leave them out for a block whose output it does not
	// need (REAPER does that while the transport is stopped), which says
	// nothing about the track and would make the answer flicker.
	Vst::SpeakerArrangement arrangement = 0;
	if (getBusArrangement (Vst::kOutput, 0, arrangement) != kResultOk)
	{
		writeBusDiagnostics (hostNameFrom (getHostContext ()), "read-arrangement-failed", 0, arrangement, -1, &data, forceMonoTrack);
		return 0;
	}

	int32 channels = channelCountOf (arrangement);

	// only when the answer changes, so the log stays readable
	if (channels != lastLoggedChannels)
	{
		lastLoggedChannels = channels;

		Vst::SpeakerArrangement inputArrangement = 0;
		getBusArrangement (Vst::kInput, 0, inputArrangement);

		writeBusDiagnostics (hostNameFrom (getHostContext ()), "process", inputArrangement, arrangement, channels, &data, forceMonoTrack);
	}

	return channels;
}

//------------------------------------------------------------------------
void SpotAlignerProcessor::reportMeters (Vst::ProcessData& data, double leftDb, double rightDb)
{
	// A meter that falls as fast as it rises cannot be read, so the release is
	// slowed to about 60 dB per second while the attack is immediate.
	const double releaseDb = currentSampleRate > 0.0 ? 60.0 * data.numSamples / currentSampleRate : 6.0;

	meterLeftDb = leftDb > meterLeftDb - releaseDb ? leftDb : meterLeftDb - releaseDb;
	meterRightDb = rightDb > meterRightDb - releaseDb ? rightDb : meterRightDb - releaseDb;

	if (!data.outputParameterChanges)
		return;

	const int32 ids[2] = {kParamMeterLeftDb, kParamMeterRightDb};
	const double values[2] = {meterLeftDb, meterRightDb};

	for (int i = 0; i < 2; ++i)
	{
		int32 index = 0;
		if (auto* queue = data.outputParameterChanges->addParameterData (ids[i], index))
		{
			int32 sampleOffset = 0;
			queue->addPoint (0, toNormalizedValue (ids[i], values[i]), sampleOffset);
		}
	}
}

//------------------------------------------------------------------------
void SpotAlignerProcessor::reportTrackChannels (Vst::ProcessData& data, int32 channels)
{
	// Only what this processor has already told the host counts as known. Its
	// own default is not the same thing: the host initialises the controller
	// side with values of its own, so the editor can sit at zero while this
	// side is perfectly happy with two. Comparing against the local default
	// instead of against what was sent is what kept the editor showing no
	// channels at all and warning about a track that was fine.
	if (channels == lastReportedChannels)
		return;

	lastReportedChannels = channels;
	parameterValues[kParamTrackChannels] = toNormalizedValue (kParamTrackChannels, static_cast<double> (channels));

	if (!data.outputParameterChanges)
		return;

	int32 index = 0;
	if (auto* queue = data.outputParameterChanges->addParameterData (kParamTrackChannels, index))
	{
		int32 sampleOffset = 0;
		queue->addPoint (0, parameterValues[kParamTrackChannels], sampleOffset);
	}
}

//------------------------------------------------------------------------
void SpotAlignerProcessor::applyParameterChanges (Vst::IParameterChanges* changes)
{
	if (!changes)
		return;

	const int32 numParamsChanged = changes->getParameterCount ();
	for (int32 index = 0; index < numParamsChanged; ++index)
	{
		Vst::IParamValueQueue* queue = changes->getParameterData (index);
		if (!queue)
			continue;

		const Vst::ParamID id = queue->getParameterId ();
		if (id >= static_cast<Vst::ParamID> (kParamCount))
			continue;

		// only the value at the end of the block is needed: the distance is a
		// static value while the transport runs
		const int32 numPoints = queue->getPointCount ();
		if (numPoints <= 0)
			continue;

		int32 sampleOffset = 0;
		Vst::ParamValue value = 0.0;
		if (queue->getPoint (numPoints - 1, sampleOffset, value) != kResultOk)
			continue;

		parameterValues[id] = value;
	}
}

//------------------------------------------------------------------------
tresult PLUGIN_API SpotAlignerProcessor::setupProcessing (Vst::ProcessSetup& newSetup)
{
	if (newSetup.symbolicSampleSize != Vst::kSample32)
		return kResultFalse;

	currentSampleRate = newSetup.sampleRate > 0.0 ? newSetup.sampleRate : 44100.0;

	// the edit controller uses this for its delay readout
	moduleSampleRate ().store (currentSampleRate);

	// size both lines for the longest distance the parameters can produce, at
	// the lowest speed of sound we expect indoors
	const double maxDelayInSamples = maxSlantDistanceMeters () / SoundSpeed::kMinimumSpeedOfSound * currentSampleRate;
	delayLeft.prepare (currentSampleRate, maxDelayInSamples);
	delayRight.prepare (currentSampleRate, maxDelayInSamples);
	delayCentre.prepare (currentSampleRate, maxDelayInSamples);

	return AudioEffect::setupProcessing (newSetup);
}

//------------------------------------------------------------------------
tresult PLUGIN_API SpotAlignerProcessor::setBusArrangements (Vst::SpeakerArrangement* inputs, int32 numIns,
                                                              Vst::SpeakerArrangement* outputs, int32 numOuts)
{
	// The object is one microphone and the result is a stereo image of it.
	// A host that can only give one output channel cannot carry that image;
	// refusing the arrangement would keep the plug-in from loading at all, so
	// it is accepted and reported instead (see process). Anything else - more
	// channels, surround - is not what this plug-in is for.
	if (numIns != 1 || numOuts != 1)
		return kResultFalse;

	const bool inputOk = inputs[0] == Vst::SpeakerArr::kMono || inputs[0] == Vst::SpeakerArr::kStereo;
	const bool outputOk = outputs[0] == Vst::SpeakerArr::kMono || outputs[0] == Vst::SpeakerArr::kStereo;

	if (!inputOk || !outputOk)
	{
		writeBusDiagnostics (hostNameFrom (getHostContext ()), "bus-arr-refused", inputs[0], numOuts > 0 ? outputs[0] : 0, -1, nullptr, false);
		return kResultFalse;
	}

	writeBusDiagnostics (hostNameFrom (getHostContext ()), "bus-arr-accepted", inputs[0], outputs[0], -1, nullptr, false);

	return AudioEffect::setBusArrangements (inputs, numIns, outputs, numOuts);
}

//------------------------------------------------------------------------
tresult PLUGIN_API SpotAlignerProcessor::canProcessSampleSize (int32 symbolicSampleSize)
{
	// by default kSample32 is supported
	if (symbolicSampleSize == Vst::kSample32)
		return kResultTrue;

	// disable the following comment if your processing support kSample64
	/* if (symbolicSampleSize == Vst::kSample64)
		return kResultTrue; */

	return kResultFalse;
}

//------------------------------------------------------------------------
tresult PLUGIN_API SpotAlignerProcessor::setState (IBStream* state)
{
	// called when we load a preset, the model has to be reloaded
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

		parameterValues[i] = std::min (1.0, std::max (0.0, value));
	}

	// a loaded state must not be polluted by what is still in the buffers, and
	// the placement it describes starts at its value
	delayLeft.clear ();
	delayRight.clear ();
	placementStarted = false;

	return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API SpotAlignerProcessor::getState (IBStream* state)
{
	// here we need to save the model
	if (!state)
		return kResultFalse;

	IBStreamer streamer (state, kLittleEndian);

	if (!streamer.writeInt32u (kStateVersion))
		return kResultFalse;

	for (int32 i = 0; i < kParamCount; ++i)
	{
		if (!streamer.writeDouble (parameterValues[i]))
			return kResultFalse;
	}

	return kResultOk;
}

//------------------------------------------------------------------------
} // namespace MyCompanyName
