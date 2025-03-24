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
    : midiNoteNumber(noteNumber), rel(rt), att(0.1f), dec(0.3f), sus(0.7f),
    squareWave(Aurora::SQUARE, sr),
    triangleWave(Aurora::TRIANGLE, sr),
    sawWave(Aurora::SAW, sr),
    env(Aurora::ads_gen(att, dec, sus), rt, Aurora::def_sr),
    osc(&sawWave,sr)
{

}

void SimpleSynthProcessor::Synth::setWaveform(int waveForm)
{
    switch(waveForm)
    {
        case 1:
            osc.waveset(&sawWave);
            return;
        case 2:
            osc.waveset(&squareWave);
            return;
        case 3:
            osc.waveset(&triangleWave);
            return;
    }
}

void SimpleSynthProcessor::Synth::setSampleRate(int sr)
{
    osc.reset(sr);
    env.reset(sr);
    sawWave.reset(Aurora::SAW, sr);
    triangleWave.reset(Aurora::TRIANGLE, sr);
    squareWave.reset(Aurora::SQUARE, sr);
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
//    for ( int i = 0 ; i < 1 ; i++)
//		synthVoices[i] = Synth(0, 1.f, 44100);
//        

    addInputBus("Input Bus", 2, lattice::ChannelLayout::Stereo);
    addOutputBus("Output Bus", 2, lattice::ChannelLayout::Stereo);

    addParameter({ "Attack", 0, 1, 0.01, 0.001, 1});
    addParameter({ "Decay", 0, 2, 0.2, 0.001, 1});
    addParameter({ "Sustain", 0, 1, 0.7, 0.001, 1});
    addParameter({ "Release", 0, 3, 0.1, 0.001, 1});

    setEditorSize(700, 300);
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
        noteEvents.pop_front();
    }

    // Iterate through all voices and sum their signals
    for (int i = 0 ; i < 16 ; i++)
    {
        auto voice = synthVoices[i];
        
    }

    
    // Copy the summed output to the output buffers
    for (std::size_t i = 0; i < blockSize; ++i)
    {
        outputs[0][i] = 0; // Left channel
        outputs[1][i] = 0; // Right channel
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
	const auto paramName = getParameters()[paramId].name;

    for (auto& voice : synthVoices)
    {
        if(paramName == "Wave")
        {
            voice.setWaveform(int(value));
        }
        else if(paramName == "Attack")
        {
            voice.setAttack(value);
        }
        else if(paramName == "Decay")
        {
            voice.setDecay(value);
        }
        else if(paramName == "Sustain")
        {
            voice.setSustain(value);
        }
        else if(paramName == "Release")
        {
            voice.setRelease(value);
        }
        else
        {
            lattice::logDebug << "Unknown parameter name: " << paramName;
        }
    }
}

void SimpleSynthProcessor::prepareToPlay(double sampleRate, uint32_t blockSize, uint32_t /*maxFrameCount*/)
{
    sr = sampleRate;
//    for (auto& voice : synthVoices)
//    {
//        voice.setBlockSize(blockSize);
//        voice.setSampleRate(sampleRate);
//    }
}

