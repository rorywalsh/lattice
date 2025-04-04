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

// Define the operating system
#if defined (_WIN32) || defined (_WIN64)
#define LATTICE_WINDOWS 1
#elif __APPLE__
#define LATTICE_MACOS 1
#elif defined (LINUX) || defined (__linux__)
#define LATTICE_LINUX 1
#endif

namespace lattice
{

class Processor
{
public:
    // Constructor: Initialise plugins with max number of parameters
    Processor() { parameters.reserve(64); }
    // Virtual destructor
    virtual ~Processor() = default;

    
    // ============= Audio Processing Methods =============
    virtual void process(float** inputs, float** outputs, std::size_t blockSize) = 0;
    virtual void prepareToPlay(double sampleRate, uint32_t minFrameCount, uint32_t maxFrameCount) = 0;
    
    
    // ============= Parameter Handling Methods =============
    virtual void setParameter(int paramId, double value) = 0;
    virtual void onMesssgeFromWebView(const nlohmann::json &j) = 0;
    
    std::vector<Parameter>& getParameters() { return parameters; }
    double getParameter(int paramId) { return getParameters()[paramId].value; }

    double getParameter(const std::string& name) {
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
    std::string getMountPoint() const {    return customMountPoint;    }
    
    // Use this to set the name of the function that will be called each time you send data to
    // you webview - it defaults to "hostMessageCallback" which is used by all the included examples
    void setWebViewSendFunctionName(const std::string& functionName){    webViewSendFunctionName = functionName;  }
    std::string getWebViewSendFunctionName() const {    return webViewSendFunctionName;    }

    // ============= Channel Configuration =================
    lattice::ChannelConfig getChannelConfig() const { return channelConfig; }
    void addInputBus(const std::string &name, int channels, lattice::ChannelLayout grouping) {
        channelConfig.addInputBus(name, channels, grouping);
    }
    void addOutputBus(const std::string& name, int channels, lattice::ChannelLayout grouping) {
        channelConfig.addOutputBus(name, channels, grouping);
    }
    
    
    // ============= Note Event Handling =================
    void addNoteEvent(lattice::NoteEvent noteEvent) { noteEvents.push_back(noteEvent); }
    std::deque<NoteEvent>& getNoteEvents() { return noteEvents; }
    
    
    // ============= State Management ============= 
    virtual nlohmann::json savePluginState() { return {}; }
    virtual void loadPluginState(nlohmann::json /*state*/) {}
    
    
    // ============= Callback Functions =============
    std::function<void(uint32_t, float)> sendParameterUpdateToHost = nullptr;
    std::function<void(nlohmann::json)> sendWebViewMessage = nullptr;
    
    
    
private:
    int editorWidth = 800;
    int editorHeight = 800;
    std::vector<Parameter> parameters;
    std::unordered_map<std::string, Parameter*> parameterMap;
    std::deque<NoteEvent> noteEvents;
    lattice::ChannelConfig channelConfig;
    std::string customMountPoint;
    std::string webViewSendFunctionName = "hostMessageCallback";
};

} // namespace lattice
