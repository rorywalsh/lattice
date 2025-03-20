// TestProcessor.cpp
#include "FlangerProcessor.h"
#include <iostream>

//===================================================================================
pluginType* LatticeProcessorPluginFactory::createPlugin(const clap_host* host)
{
    //create a new instance of GainProcessor 
    auto* processor = new FlangerProcessor();
    return new pluginType(host, *processor);
}
//===================================================================================

FlangerProcessor::FlangerProcessor()
    : Processor(), flangerLeft(10, 44100), flangerRight(10, 44100)
{

    addInputBus("Input Bus", 2, lattice::ChannelLayout::Stereo);

    addOutputBus("Output Bus", 2, lattice::ChannelLayout::Stereo);

    addParameter({ "Max Delay", 0, 5, 2.5f, 0.1f, 1.f});
    addParameter({ "LFO Frequency", 0, 20, .5, .0001f, 1.f});
    addParameter({ "Feedback", 0, 1, .7f, .0001f, 1.f});
    addParameter({ "Gain", 0, 1, .5, .01f, 1.f});

    setEditorSize(600, 300);
}


void FlangerProcessor::process(float** inputs, float** outputs, std::size_t blockSize)
{
    // Resize the input buffers to match the block size
    inputLeft.resize(blockSize);
    inputRight.resize(blockSize);
    
    std::copy(inputs[0], inputs[0] + blockSize, inputLeft.begin());
    std::copy(inputs[1], inputs[1] + blockSize, inputRight.begin());
    
    const float lfoFreq = getParameter("LFO Frequency");
    const float fdb = getParameter("Feedback");
    const float gain = getParameter("Gain");
    const float maxdel = getParameter("Max Delay");
    
    // Apply the flanger effect to the input data
    auto &outL = flangerLeft(inputLeft, lfoFreq, fdb, gain, maxdel / 1000.f);
    auto &outR = flangerRight(inputRight,  lfoFreq, fdb, gain, maxdel / 1000.f);
    
    // Copy the processed data to the output buffers
    std::copy(outL.begin(), outL.end(), outputs[0]);
    std::copy(outR.begin(), outR.end(), outputs[1]);


}

nlohmann::json FlangerProcessor::savePluginState()
{	
	const auto parameters = getParameters();
    nlohmann::json j;
    
    // Running through these by index...
    for(size_t i = 0 ; i < parameters.size() ; i++)
    {
        j[parameters[i].name] = parameters[i].value;
    }
    
    return j;
};

void FlangerProcessor::loadPluginState(nlohmann::json state)
{
    auto json = nlohmann::json::parse(state.dump(4));
    int idx = 0;
    
    for (auto& [key, value] : json.items()) {
        
        // Send value to host when project is loaded
        sendParameterUpdateToHost(idx, value.get<float>());
        
        nlohmann::json j, data;
        j["command"] = "parameterChange";
        data["paramIdx"] = idx;
        data["value"] = value.get<float>();
        j["data"] = data;
        
        
        // Saves state data to message queue.
        // The data will be sent when the webview opens (see onEditorLoad()).
        webviewMessageQueue.push_back(j);
        idx++;
    }
};

void FlangerProcessor::onMesssgeFromWebView(const nlohmann::json& j)
{
    auto json = j.at(0);

    if(json.get<std::string>() == "onEditorLoad")
    {
        for(const auto &msg : webviewMessageQueue)
        {
            lattice::logDebug << msg.dump(4);
            sendWebViewMessage(msg.dump());
        }
    }
    else
    {
        float value = json.value("value", 0.f);
        auto paramIdx = json.value("paramIdx", -1);
        sendParameterUpdateToHost(paramIdx, value);
    }
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
