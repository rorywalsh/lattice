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
#include <vector>
#include <unordered_map>
#include <list>
#include <deque>
#include <iostream>
#include <functional>

// External Library Headers
#include <lattice/clap/LatticeClapPlugin.h>
#include <nlohmann/json.hpp>

// Project Headers
#include "LatticeStructs.h"


namespace lattice
{

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
    void addInputBus(const std::string &name, int channels, lattice::ChannelLayout grouping) {
        channelConfig.addInputBus(name, channels, grouping);
    }
    void addOutputBus(const std::string& name, int channels, lattice::ChannelLayout grouping) {
        channelConfig.addOutputBus(name, channels, grouping);
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
    std::function<void(std::string)> setWebViewHtml;
   
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
