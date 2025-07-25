// TestProcessor.cpp
#include "GainProcessor.h"
#include <iostream>

//===================================================================================
pluginType* LatticeProcessorPluginFactory::createPlugin(const clap_host* host)
{
    //create a new instance of GainProcessor 
    auto* processor = new GainProcessor();
    return new pluginType(host, *processor);
}
//===================================================================================

GainProcessor::GainProcessor()
    : Processor()
{
	addInputBus("Input Bus", 2, lattice::ChannelLayout::Stereo);
    addOutputBus("Output Bus", 2, lattice::ChannelLayout::Stereo);

    addParameter({ "Gain", 0, 1 });

    setEditorSize(400, 300);
}

void GainProcessor::process(double** inputs, double** outputs, std::size_t blockSize)
{

	const auto channels = getChannelConfig().getTotalNumInputChannels();
    
    for (uint32_t i = 0; i < blockSize; i++)
    {
        for (uint32_t ch = 0; ch < channels; ++ch)
        {
            outputs[ch][i] = inputs[ch][i]*getParameterValue("Gain");
        }
    }
}

void GainProcessor::onMessageFromWebView(const nlohmann::json& j)
{
    std::cout << j.at(0).dump(4);
    float value = j.at(0).value("value", 0.f);
    auto paramIdx = j.at(0).value("paramIdx", -1);
    auto gesture = j.value("gesture", "gesture");

    if (gesture == "begin") {
        addParameterChange({paramIdx, getParameter(paramIdx).toNormalised(value), lattice::ParamChangeType::GestureBegin});
    } else if (gesture == "value") {
        addParameterChange({paramIdx, getParameter(paramIdx).toNormalised(value), lattice::ParamChangeType::Value});
    } else if (gesture == "end") {
        addParameterChange({paramIdx, getParameter(paramIdx).toNormalised(value), lattice::ParamChangeType::GestureEnd});
    }
    
    getParameters()[paramIdx].value = value;
}

// This can be called from the host - if so update the
// corresponding parameter value using updateParameter() function
void GainProcessor::setParameter(int paramId, double value)
{
    getParameters()[paramId].value = getParameter(paramId).fromNormalised(value);
}

void GainProcessor::prepareToPlay(double /*sampleRate*/, uint32_t /*minFrameCount*/, uint32_t /*maxFrameCount*/)
{
    
}
