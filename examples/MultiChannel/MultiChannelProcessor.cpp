// TestProcessor.cpp
#include "MultiChannelProcessor.h"
#include <iostream>

//===================================================================================
pluginType* LatticeProcessorPluginFactory::createPlugin(const clap_host* host)
{
    //create a new instance of GainProcessor 
    auto* processor = new MultiChannelProcessor();
    return new pluginType(host, *processor);
}
//===================================================================================

MultiChannelProcessor::MultiChannelProcessor()
    : Processor()
{
    addInputBus("Input Bus", 2, lattice::ChannelLayout::Stereo);
    addInputBus("Input Bus", 2, lattice::ChannelLayout::Stereo);

    addOutputBus("Output Bus", 2, lattice::ChannelLayout::Stereo);
    addOutputBus("Output Bus", 2, lattice::ChannelLayout::Stereo);

    addParameter({"Gain 1", 0, .8, .2, 0.01, 1});
    addParameter({"Gain 2", 0, .8, .2, 0.01, 1});
    addParameter({"Gain 3", 0, .8, .2, 0.01, 1});
    addParameter({"Gain 4", 0, .8, .2, 0.01, 1});

    setEditorSize(600, 300);
}

void MultiChannelProcessor::process(float** inputs, float** outputs, std::size_t blockSize)
{
    
    for (uint32_t i = 0; i < blockSize; i++)
    {
        outputs[0][i] = inputs[0][i] * getParameterValue("Gain 1");
        outputs[1][i] = inputs[1][i] * getParameterValue("Gain 2");
        outputs[2][i] = inputs[2][i] * getParameterValue("Gain 3");
        outputs[3][i] = inputs[3][i] * getParameterValue("Gain 4");
    }
}

void MultiChannelProcessor::onMessageFromWebView(const nlohmann::json &j)
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
void MultiChannelProcessor::setParameter(int paramId, double value)
{
    getParameters()[paramId].value = value;
}

void MultiChannelProcessor::prepareToPlay(double /*sampleRate*/, uint32_t /*minFrameCount*/, uint32_t /*maxFrameCount*/)
{
    
}
