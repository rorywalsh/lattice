/*
    MIT License

    Copyright (c) 2025 Rory Walsh

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

#pragma once

// Standard Library Headers
#include <cstddef>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <list>
#include <deque>
#include <iostream>
#include <functional>
#include <sstream>

// External Library Headers
#include <lattice/clap/LatticeClapPlugin.h>
#include <nlohmann/json.hpp>

// Project Headers
#include "LatticeStructs.h"

namespace ARA {
namespace PlugIn {
class DocumentController;
}
}

namespace lattice
{
    struct AraAnalysisJob
    {
        uint64_t jobId = 0;
        std::string sourceId;
        std::string regionId;
        std::string analysisType;
        double startSeconds = 0.0;
        double durationSeconds = 0.0;
        nlohmann::json metadata = {};
    };

    struct AraAnalysisResult
    {
        uint64_t jobId = 0;
        std::string sourceId;
        std::string regionId;
        std::string analysisType;
        bool success = false;
        nlohmann::json payload = {};
        std::string errorMessage;
    };

class Processor
{
public:
    // Constructor: Initialise plugins with max number of parameters
    Processor() {
        parameters.reserve(64);
        // Initialize callbacks to prevent bad function call exceptions
        setWebViewHtml = [](std::string) {};
        sendWebViewMessage = [](nlohmann::json) {};
        addParameterChange = [](lattice::ParameterChange) {};
        addOutputNoteEvent = [](lattice::OutputNoteEvent) {};
        addRawMidiEvent = [](lattice::RawMidiEvent) {};
        araCallback = [](const ARA::PlugIn::DocumentController*, const nlohmann::json&) {};
        enqueueAraAnalysisJob = [](const lattice::AraAnalysisJob&) {};
        publishAraAnalysisResult = [](const lattice::AraAnalysisResult&) {};
        getAraFactory = []() { return nullptr; };
        bindToAraDocumentController = [](void*, uint32_t, uint32_t) { return static_cast<const void*>(nullptr); };
        requestGuiResize = [](uint32_t, uint32_t) { return false; };
        applyGuiResize = [](uint32_t, uint32_t) {};
    }
    // Virtual destructor
    virtual ~Processor() = default;
    
    // ============= Audio Processing Methods =============
    virtual void process(float** inputs, float** outputs, std::size_t blockSize) = 0;
    virtual void prepareToPlay(double sampleRate, uint32_t minFrameCount, uint32_t maxFrameCount) = 0;
    
   
    // ============= Parameter Handling Methods =============
    virtual void setParameter(int paramId, double value) = 0;
    virtual void onMessageFromWebView(const nlohmann::json &j) = 0;
    
    // Return the full vector of parameters
    std::vector<Parameter>& getParameters() { return parameters; }
    
    // Access parameter object
    Parameter& getParameter(int paramId) { return getParameters()[paramId]; }
    Parameter* getParameter(const std::string& name) {
        auto it = parameterMap.find(name);
        return (it != parameterMap.end()) ? it->second : nullptr;
    }
    
    // Return parameter values
    double getParameterValue(int paramId) { return getParameters()[paramId].value; }
    double getParameterValue(const std::string& name) {
        auto it = parameterMap.find(name);
        return (it != parameterMap.end()) ? it->second->value : 0.0;
    }
    
    void addParameter(lattice::Parameter parameter) {
        parameters.push_back(parameter);
        parameterMap[parameter.name] = &parameters.back(); // Fast lookup
    }
    
    // ============= GUI Methods =================
    void setEditorSize(int w, int h) { editorWidth = w; editorHeight = h; }
    int getEditorWidth() const { return editorWidth; }
    int getEditorHeight() const { return editorHeight; }
    
    // Use this to set a custom mount point - otherwise use bundled Resources folder
    void setMountPoint(const std::string& mntPoint){ customMountPoint = mntPoint; }
    std::string getMountPoint() const   {    return customMountPoint;   }
    
    // Gets/sets server url - a unique address use to server web frontend
    void setServerUrl(const std::string& url){    serverUrl = url;      }
    std::string getServerUrl() const    {    return serverUrl;          }

    // Use this to set the name of the function that will be called each time you send data to
    // you webview - it defaults to "hostMessageCallback" which is used by all the included examples
    void setWebViewSendFunctionName(const std::string& functionName){    webViewSendFunctionName = functionName;  }
    std::string getWebViewSendFunctionName() const {    return webViewSendFunctionName;    }

    // Override to get notifed when webview is ready
    virtual void onWebViewIsReady(){};
    
    // ============= Channel Configuration =================
    lattice::ChannelConfig getChannelConfig() const { return channelConfig; }

    // Declare a named audio port configuration. Call once per supported layout from
    // the processor constructor. The first call becomes the default (ID 0).
    //
    // `ins` / `outs` are '+'-separated channel counts, one number per bus.
    //   "2"     → single stereo bus
    //   "2+1"   → main stereo bus + mono sidechain bus
    //   "2+2"   → two stereo buses
    void addAudioPortsConfig(const std::string& name, const std::string& ins, const std::string& outs)
    {
        auto parseCounts = [](const std::string& s) {
            std::vector<int> counts;
            std::istringstream ss(s);
            std::string token;
            while (std::getline(ss, token, '+'))
                if (!token.empty()) counts.push_back(std::stoi(token));
            return counts;
        };

        lattice::AudioPortsConfig cfg;
        cfg.id   = static_cast<uint32_t>(channelConfig.configs.size());
        cfg.name = name;

        auto inCounts = parseCounts(ins);
        for (size_t i = 0; i < inCounts.size(); ++i)
            cfg.inputBuses.emplace_back("Input Bus " + std::to_string(i), inCounts[i]);

        auto outCounts = parseCounts(outs);
        for (size_t i = 0; i < outCounts.size(); ++i)
            cfg.outputBuses.emplace_back("Output Bus " + std::to_string(i), outCounts[i]);

        channelConfig.configs.push_back(std::move(cfg));
    }

    // Called by the CLAP backend when the host selects a different port config.
    void selectAudioPortsConfig(uint32_t id)
    {
        for (uint32_t i = 0; i < channelConfig.configs.size(); ++i)
        {
            if (channelConfig.configs[i].id == id)
            {
                channelConfig.activeConfigIndex = i;
                return;
            }
        }
    }
    
    // ============= Note Event Handling =================
    void addNoteEvent(const lattice::NoteEvent& noteEvent) { noteEvents.push_back(noteEvent); }
    std::deque<NoteEvent>& getNoteEvents() { return noteEvents; }
    
    // ============= Transport Info =================
    const lattice::TransportInfo& getTransportInfo() const { return transportInfo; }
    void setTransportInfo(const lattice::TransportInfo& info) { transportInfo = info; }
    
    // ============= State Management ============= 
    virtual nlohmann::json savePluginState() { return {}; }
    virtual void loadPluginState(nlohmann::json /*state*/) {}
    
    // ============= MIDI Output Helper Methods =============
    // High-level note output (uses CLAP note events)
    void sendNoteOn(int16_t key, double velocity, uint32_t sampleOffset, int16_t channel = 0, int32_t noteId = -1) {
        addOutputNoteEvent({lattice::NoteEventType::noteOn, channel, key, velocity, noteId, sampleOffset});
    }
    void sendNoteOff(int16_t key, double velocity, uint32_t sampleOffset, int16_t channel = 0, int32_t noteId = -1) {
        addOutputNoteEvent({lattice::NoteEventType::noteOff, channel, key, velocity, noteId, sampleOffset});
    }

    // Raw MIDI output helpers (uses CLAP MIDI events)
    void sendCC(uint8_t cc, uint8_t value, uint32_t sampleOffset, uint8_t channel = 0) {
        addRawMidiEvent({static_cast<uint8_t>(0xB0 | (channel & 0x0F)), cc, value, 3, sampleOffset});
    }
    void sendProgramChange(uint8_t program, uint32_t sampleOffset, uint8_t channel = 0) {
        addRawMidiEvent({static_cast<uint8_t>(0xC0 | (channel & 0x0F)), program, 0, 2, sampleOffset});
    }
    void sendPitchBend(int16_t bend, uint32_t sampleOffset, uint8_t channel = 0) {
        // bend: -8192 to 8191, centered at 0
        uint16_t val = static_cast<uint16_t>(bend + 8192);
        addRawMidiEvent({static_cast<uint8_t>(0xE0 | (channel & 0x0F)),
                         static_cast<uint8_t>(val & 0x7F),
                         static_cast<uint8_t>((val >> 7) & 0x7F), 3, sampleOffset});
    }
    void sendAftertouch(uint8_t pressure, uint32_t sampleOffset, uint8_t channel = 0) {
        addRawMidiEvent({static_cast<uint8_t>(0xD0 | (channel & 0x0F)), pressure, 0, 2, sampleOffset});
    }
    void sendPolyAftertouch(uint8_t key, uint8_t pressure, uint32_t sampleOffset, uint8_t channel = 0) {
        addRawMidiEvent({static_cast<uint8_t>(0xA0 | (channel & 0x0F)), key, pressure, 3, sampleOffset});
    }
    // Raw MIDI passthrough (e.g., from Csound's WriteMidiData)
    void sendRawMidi(const unsigned char* buffer, int numBytes, uint32_t sampleOffset) {
        addRawMidiEvent({buffer, numBytes, sampleOffset});
    }

    // ============= Callback Functions =============
    std::function<void(nlohmann::json)> sendWebViewMessage;
    std::function<void(lattice::ParameterChange)> addParameterChange;
    std::function<void(lattice::OutputNoteEvent)> addOutputNoteEvent;  ///< Callback to output MIDI notes to host
    std::function<void(lattice::RawMidiEvent)> addRawMidiEvent;        ///< Callback to output raw MIDI to host
    std::function<void(const ARA::PlugIn::DocumentController* documentController, const nlohmann::json& event)> araCallback; ///< Callback for ARA lifecycle/analysis events
    std::function<void(const lattice::AraAnalysisJob&)> enqueueAraAnalysisJob; ///< Callback to enqueue ARA analysis jobs
    std::function<void(const lattice::AraAnalysisResult&)> publishAraAnalysisResult; ///< Callback to publish completed ARA results
    std::function<const void*()> getAraFactory; ///< Returns ARA factory pointer when ARA is supported
    std::function<const void*(void* documentControllerRef, uint32_t knownRoles, uint32_t assignedRoles)> bindToAraDocumentController; ///< Binds plugin instance to an ARA document controller
    std::function<void(std::string)> setWebViewHtml;
    std::function<bool(uint32_t width, uint32_t height)> requestGuiResize;  ///< Callback to request GUI resize from host
    std::function<void(uint32_t width, uint32_t height)> applyGuiResize;    ///< Callback to actually resize the GUI (for hosts that don't call guiSetSize)

private:
    int editorWidth = 800;
    int editorHeight = 800;
    std::vector<Parameter> parameters;
    std::unordered_map<std::string, Parameter*> parameterMap;
    std::deque<NoteEvent> noteEvents;
    lattice::ChannelConfig channelConfig;
    lattice::TransportInfo transportInfo;
    std::string customMountPoint = "";
    std::string serverUrl = "";
    std::string webViewSendFunctionName = "hostMessageCallback";
};

} // namespace lattice
