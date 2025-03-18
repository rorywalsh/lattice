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
    setEditorSize(400, 300);
}

void GainProcessor::process(float** inputs, float** outputs, std::size_t blockSize)
{
    const auto channels = getNumInputs();
    
    for (uint32_t i = 0; i < blockSize; i++)
    {
        for (uint32_t ch = 0; ch < channels; ++ch)
            outputs[ch][i] = inputs[ch][i]*getParameter("Gain");
    }
}

void GainProcessor::onMesssgeFromWebView(nlohmann::json j)
{
    std::cout << j.at(0).dump(4);
    float value = j.at(0).value("value", 0.f);
    auto paramIdx = j.at(0).value("paramIdx", -1);
    sendParameterUpdateToHost(paramIdx, value);
}

// This can be called from the host - if so update the
// corresponding parameter value using updateParameter() function
void GainProcessor::setParameter(int paramId, double value)
{
    getParameters()[paramId].value = value;
}

void GainProcessor::prepareToPlay(double /*sampleRate*/, uint32_t /*minFrameCount*/, uint32_t /*maxFrameCount*/)
{
    
}
