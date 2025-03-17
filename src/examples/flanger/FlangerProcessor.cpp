// TestProcessor.cpp
#include "FlangerProcessor.h"
#include <iostream>

//===================================================================================
pluginType* LatticeProcessorPluginFactory::createPlugin(const clap_host* host)
{
    //create a new instance of GainProcessor 
    auto* processor = new FlangerProcessor(2, 2);
    return new pluginType(host, *processor);
}
//===================================================================================

FlangerProcessor::FlangerProcessor(int numInputs, int numOutputs)
    : Processor(numInputs, numOutputs), flangerLeft(10, 44100), flangerRight(10, 44100)
{
    addParameter({ "Max Delay", 0, 5, 2.5f, 0.1f, 1.f});
    addParameter({ "LFO Frequency", 0, 20, .5f, .0001f, 1.f});
    addParameter({ "Feedback", 0, 1, .7f, .0001f, 1.f});
    addParameter({ "Gain", 0, 1, .5, .01f, 1.f});
    setEditorSize(600, 300);
}

void FlangerProcessor::process(float** inputs, float** outputs, std::size_t blockSize)
{
    // Resize the input buffers to match the block size
    inputLeft.resize(blockSize);
    inputRight.resize(blockSize);
    
    // Working with mono input for now
    std::copy(inputs[0], inputs[0] + blockSize, inputLeft.begin());
//    std::copy(inputs[1], inputs[1] + blockSize, inputRight.begin());

    // Apply the flanger effect to the input data
    auto &outL = flangerLeft(inputLeft, getParameter("LFO Frequency"), getParameter("Feedback"), getParameter("Gain"), getParameter("Max Delay") / 1000.f);
    auto &outR = flangerRight(inputRight, getParameter("LFO Frequency"), getParameter("Feedback"), getParameter("Gain"), getParameter("Max Delay") / 1000.f);
    
    // Copy the processed data to the output buffers
    std::copy(outL.begin(), outL.end(), outputs[0]);
    std::copy(outL.begin(), outL.end(), outputs[1]);


}

void FlangerProcessor::onMesssgeFromWebView(nlohmann::json j)
{
    std::cout << j.at(0).dump(4);
    float value = j.at(0).value("value", 0.f);
    auto paramIdx = j.at(0).value("paramIdx", -1);
    sendParameterUpdateToHost(paramIdx, value);
}

void FlangerProcessor::setParameter(int paramId, double value)
{
    getParameters()[paramId].value = value;
}

void FlangerProcessor::prepareToPlay(double sampleRate, uint32_t /*minFrameCount*/, uint32_t /*maxFrameCount*/)
{
    flangerLeft.reset(sampleRate);
    flangerRight.reset(sampleRate);
}
