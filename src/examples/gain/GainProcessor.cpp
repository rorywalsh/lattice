// TestProcessor.cpp
#include "GainProcessor.h"
#include <iostream>

//===================================================================================
pluginType* LatticeProcessorPluginFactory::createPlugin(const clap_host* host)
{
    //create a new instance of GainProcessor 
    auto* processor = new GainProcessor(2, 2);
    return new pluginType(host, *processor);
}
//===================================================================================

GainProcessor::GainProcessor(int numInputs, int numOutputs)
    : Processor(numInputs, numOutputs)
{
    addParameter({"Gain", 0, 1});
}

void GainProcessor::process(float** inputs, float** outputs, std::size_t blockSize)
{
    const auto channels = getNumInputs();
    auto& noteEvents = getNoteEvents();
    
    while (!noteEvents.empty()) {
        auto event = noteEvents.front();
        event.log();
        noteEvents.pop_front();
    }
    
    for (uint32_t i = 0; i < blockSize; i++)
    {
        for (uint32_t ch = 0; ch < channels; ++ch)
            outputs[ch][i] = inputs[ch][i]*getParameter(0);
    }
}

void GainProcessor::onMesssgeFromWebView(nlohmann::json j)
{
    std::cout << j.at(0).dump(4);
    float value = j.at(0).value("value", 0.f);
    auto paramIdx = j.at(0).value("paramIdx", -1);
    sendParameterUpdateToHost(paramIdx, value);
}

void GainProcessor::setParameter(int paramId, double value)
{
    getParameters()[paramId].value = value;
}

double GainProcessor::getParameter(int paramId)
{
    return getParameters()[paramId].value;
}

void GainProcessor::prepareToPlay(double /*sampleRate*/, uint32_t /*minFrameCount*/, uint32_t /*maxFrameCount*/)
{
    
}
