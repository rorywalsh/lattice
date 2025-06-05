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
    
    const float lfoFreq = getParameterValue("LFO Frequency");
    const float fdb = getParameterValue("Feedback");
    const float gain = getParameterValue("Gain");
    const float maxdel = getParameterValue("Max Delay");
    
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
    
    // Use array instead of object so we can maintain order
    nlohmann::json j = nlohmann::json::array();

    // Store parameters by index in array
    for (size_t i = 0; i < parameters.size(); i++)
    {
        nlohmann::json param;
        param["name"] = parameters[i].name;
        param["value"] = parameters[i].value;
        j.push_back(param);

        lattice::logInfo << "Name:" << parameters[i].name << " Value:" << parameters[i].value;
    }

    return j;
}

void FlangerProcessor::loadPluginState(nlohmann::json state)
{
    auto json = nlohmann::json::parse(state.dump(4));
    lattice::logDebug << json.dump(4);
    int idx = 0;

    // Iterate through array
    for (const auto& param : json)
    {
        // Read values
        float value = param.value("value", 0.f);

        // Add parameter updates to queue for host - these should be normalised
        addParameterChange({idx, getParameter(idx).toNormalised(value), lattice::ParamChangeType::Value});
        // Set parameter values for plugin
        getParameters()[idx].value = value;
        
        // Save to message queue - denormalise value for webview
        webviewMessageQueue.push_back(getParameterJson(idx, getParameter(idx).fromNormalised(value)));
        idx++;
    }
}

nlohmann::json FlangerProcessor::getParameterJson(size_t index, float value)
{
    // Prepare message for webview
    nlohmann::json j, data;
    j["command"] = "parameterChange";
    data["paramIdx"] = index;
    data["value"] = value;
    j["data"] = data;
    return j;
}

void FlangerProcessor::onMessageFromWebView(const nlohmann::json& j)
{
    try
    {
        auto json = j.at(0);

        // Check if json is a string
        if (json.is_string() && json.get<std::string>() == "onEditorLoad")
        {
            // If editor has just loaded - check for messages in queue..
            for (const auto &msg : webviewMessageQueue)
            {
                sendWebViewMessage(msg.dump());
            }
            
            const auto parameters = getParameters();
            
            // Now update the UI so it matches the current state
            // Values will typically be normalised
            for (size_t i = 0; i < parameters.size(); i++)
            {
                sendWebViewMessage(getParameterJson(i, parameters[i].value).dump());
            }
            
        }
        else if (json.is_object())
        {
            float value = json.value("value", 0.f);
            auto paramIdx = json.value("paramIdx", -1);
            auto gesture = json.value("gesture", "gesture");

            if (gesture == "begin") {
                addParameterChange({paramIdx, getParameter(paramIdx).toNormalised(value), lattice::ParamChangeType::GestureBegin});
            } else if (gesture == "value") {
                addParameterChange({paramIdx, getParameter(paramIdx).toNormalised(value), lattice::ParamChangeType::Value});
            } else if (gesture == "end") {
                addParameterChange({paramIdx, getParameter(paramIdx).toNormalised(value), lattice::ParamChangeType::GestureEnd});
            }
            
            getParameters()[paramIdx].value = value;
        }
        else
        {
            lattice::logError << "Unexpected message format: " << json.dump();
        }
    }
    catch (const nlohmann::json::exception &e)
    {
        lattice::logError << "JSON Error: " << e.what();
    }
}

// This is called from the host. 'value' will be normalised
void FlangerProcessor::setParameter(int paramId, double value)
{
    getParameters()[paramId].value = value;
    // We will send a normalised value to our web frontend
    sendWebViewMessage(getParameterJson(paramId, getParameter(paramId).fromNormalised(value)).dump());
    lattice::logDebug << "Value:" << getParameter(paramId).fromNormalised(value);
}

void FlangerProcessor::prepareToPlay(double sampleRate, uint32_t /*minFrameCount*/, uint32_t /*maxFrameCount*/)
{
    flangerLeft.reset(sampleRate);
    flangerRight.reset(sampleRate);
}
