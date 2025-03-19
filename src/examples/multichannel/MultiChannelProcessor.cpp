// TestProcessor.cpp
#include "MultiChannelProcessor.h"
#include <iostream>

//===================================================================================
pluginType* LatticeProcessorPluginFactory::createPlugin(const clap_host* host)
{
    //create a new instance of GainProcessor 
    auto* processor = new MultiChannelProcessor(4, 4);
    return new pluginType(host, *processor);
}
//===================================================================================

MultiChannelProcessor::MultiChannelProcessor(int numInputs, int numOutputs)
    : Processor(numInputs, numOutputs)
{
    addParameter({"Gain 1", 0, 1, .2, 0.01, 1});
    addParameter({"Gain 2", 0, 1, .2, 0.01, 1});
    addParameter({"Gain 3", 0, 1, .2, 0.01, 1});
    addParameter({"Gain 4", 0, 1, .2, 0.01, 1});
    setEditorSize(600, 300);
}

void MultiChannelProcessor::process(float** inputs, float** outputs, std::size_t blockSize)
{
    const auto channels = getNumInputs();
    
    for (uint32_t i = 0; i < blockSize; i++)
    {
        for (uint32_t ch = 0; ch < channels; ++ch)
            outputs[ch][i] = inputs[ch][i]*getParameter("Gain");
    }
}

void MultiChannelProcessor::onMesssgeFromWebView(nlohmann::json j)
{
    std::cout << j.at(0).dump(4);
    float value = j.at(0).value("value", 0.f);
    auto paramIdx = j.at(0).value("paramIdx", -1);
    sendParameterUpdateToHost(paramIdx, value);
}

// This can be called from the host - if so update the
// corresponding parameter value using updateParameter() function
void MultiChannelProcessor::setParameter(int paramId, double value)
{
    getParameters()[paramId].value = value;
}

void MultiChannelProcessor::prepareToPlay(double /*sampleRate*/, uint32_t /*minFrameCount*/, uint32_t /*maxFrameCount*/)
{
    
}
