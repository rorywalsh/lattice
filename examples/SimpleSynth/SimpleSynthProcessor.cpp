// TestProcessor.cpp
#include "SimpleSynthProcessor.h"
#include <iostream>

//===================================================================================
pluginType* LatticeProcessorPluginFactory::createPlugin(const clap_host* host)
{
    //create a new instance of SimpleSynthProcessor 
    auto* processor = new SimpleSynthProcessor();
    return new pluginType(host, *processor);
}
//===================================================================================
// Synth methods
//===================================================================================
SimpleSynthProcessor::Synth::Synth(int noteNumber, float rt, float sr)
    : midiNoteNumber(noteNumber), att(0.1f), dec(0.3f), sus(0.7f), rel(rt), wave(Aurora::def_ftlen),
    env(Aurora::ads_gen(att, dec, sus), rt, Aurora::def_sr),
    osc(&wave, sr)
{
    std::size_t n = 0;
    for (auto& s : wave) {
        s = std::sin((Aurora::twopi / wave.size()) * n++);
    }
}

void SimpleSynthProcessor::Synth::setSampleRate(int sr)
{
    osc.reset(sr);
    env.reset(sr);
}

void SimpleSynthProcessor::Synth::setBlockSize(std::size_t blockSize)
{
    osc.vsize(blockSize);
    env.vsize(blockSize);
}

//===================================================================================
// Main processor methods
//===================================================================================
SimpleSynthProcessor::SimpleSynthProcessor()
    : Processor()
{
    for ( int i = 0 ; i < 16 ; i++)
		synthVoices.push_back(Synth(0, 1.f, 44100));

    addInputBus("Input Bus", 2, lattice::ChannelLayout::Stereo);
    addOutputBus("Output Bus", 2, lattice::ChannelLayout::Stereo);

    addParameter({ "Attack", 0, 1, 0.01, 0.001, 1});
    addParameter({ "Decay", 0, 2, 0.2, 0.001, 1});
    addParameter({ "Sustain", 0, 1, 0.7, 0.001, 1});
    addParameter({ "Release", 0, 3, 0.1, 0.001, 1});

    setEditorSize(700, 300);
}

void SimpleSynthProcessor::manageVoices(lattice::NoteEvent noteEvent)
{
    bool foundNote = false;

    // First, check if the voice with the given key already exists
    for (auto& v : synthVoices)
    {
        if (v.getNoteNumber() == noteEvent.key)
        {
            foundNote = true;

            if (noteEvent.type == lattice::NoteEvent::Type::noteOn)
            {
                v.setNoteType(true);
            }
            else if (noteEvent.type == lattice::NoteEvent::Type::noteOff)
            {
                v.setNoteType(false);
            }

            // Exit the loop since we found the voice
            break;
        }
    }

    // If the voice with the given key was not found, find an inactive voice
    if (!foundNote && noteEvent.type == lattice::NoteEvent::Type::noteOn)
    {
        for (auto& v : synthVoices)
        {
            if (!v.getNoteType()) // Check if the voice is inactive (noteType is false)
            {
                // Assign the key and velocity from the noteEvent
                v.setNoteNumber(noteEvent.key);
                v.setSustain(noteEvent.velocity);
                v.setNoteType(true); // Activate the voice

                // Exit the loop after assigning the first available inactive voice
                break;
            }
        }
    }
}


//====================================================================================
void SimpleSynthProcessor::process(float** /*inputs*/, float** outputs, std::size_t blockSize)
{
    const auto channels = getChannelConfig().getTotalNumInputChannels();
    auto& noteEvents = getNoteEvents();

    // Process all note events
    while (!noteEvents.empty()) {
        auto event = noteEvents.front();
        event.log();
        manageVoices(event); // Handle the note event
        noteEvents.pop_front();
    }

    // Initialize a buffer to hold the summed output signal
    std::vector<float> mixDown(blockSize, 0.0f); // Initialize with zeros

    // Iterate through all voices and sum their signals
    for (auto& voice : synthVoices)
    {
        if (voice.getNoteType()) // Check if the voice is active
        {
            // Generate the output signal for this voice
            const std::vector<float>& voiceOutput = voice(
                voice.getSustain(),
                getMidiNoteInHertz(voice.getNoteNumber()),
                voice.getNoteType()
            );

            // Sum the voice's output into the mix buffer
            for (std::size_t i = 0; i < blockSize; ++i)
            {
                mixDown[i] += voiceOutput[i];
            }
        }
    }

    // Copy the summed output to the output buffers
    for (std::size_t i = 0; i < blockSize; ++i)
    {
        outputs[0][i] = mixDown[i]; // Left channel
        outputs[1][i] = mixDown[i]; // Right channel
    }
}

void SimpleSynthProcessor::onMesssgeFromWebView(const nlohmann::json& j)
{
    const auto json = j.at(0);
    float value = json.value("value", 0.f);
    auto paramIdx = json.value("paramIdx", -1);
    sendParameterUpdateToHost(paramIdx, value);
}

// This can be called from the host - if so, update the
// corresponding parameter value using updateParameter() function
void SimpleSynthProcessor::setParameter(int paramId, double value)
{
	const auto parameterName = getParameters()[paramId].name;

	if (parameterName == "Attack")
	{
        for (auto& voice : synthVoices)
            voice.setAttack(value);
	}
	else if (parameterName == "Decay")
	{
        for (auto& voice : synthVoices)
            voice.setDecay(value);
	}
	else if (parameterName == "Sustain")
	{
        for (auto& voice : synthVoices)
            voice.setSustain(value);
	}
	else if (parameterName == "Release")
	{
        for (auto& voice : synthVoices)
            voice.setRelease(value);
	}
	else
	{
		lattice::logDebug << "Unknown parameter name: " << parameterName;
	}
}

void SimpleSynthProcessor::prepareToPlay(double sampleRate, uint32_t blockSize, uint32_t /*maxFrameCount*/)
{
    sr = sampleRate;
    for (auto& voice : synthVoices)
        voice.setBlockSize(blockSize);
}

