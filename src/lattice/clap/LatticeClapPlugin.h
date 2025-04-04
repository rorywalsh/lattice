#pragma once

#include "platform/choc_DisableAllWarnings.h"
#include <clap/helpers/host-proxy.hxx>
#include <clap/helpers/plugin.hxx>
#include <clap/ext/params.h>
#include "platform/choc_ReenableAllWarnings.h"
#include "gui/choc_WebView.h"
#include "../LatticeServer.h"
#include "../LatticeUtils.h"
#include <deque>

// Forward declare lattice::Processor 
// and LatticeClapPlugin
class LatticeClapPlugin;

namespace lattice{
    class Processor;
}

using pluginType = LatticeClapPlugin;

class LatticeClapPlugin : public clap::helpers::Plugin<clap::helpers::MisbehaviourHandler::Ignore,
                                            clap::helpers::CheckingLevel::Maximal>
{
    

    
public:
    LatticeClapPlugin(const clap_host* host, lattice::Processor& processor);
    ~LatticeClapPlugin() override;

    
    bool audioPortsInfo(uint32_t index, bool isInput, clap_audio_port_info* info) const noexcept override;
    bool paramsInfo(uint32_t paramIndex, clap_param_info* info) const noexcept override;
    bool notePortsInfo(uint32_t index, bool isInput, clap_note_port_info *info) const noexcept override;
    
    uint32_t paramsCount() const noexcept override;
    uint32_t audioPortsCount(bool /*isInput*/) const noexcept override;

    uint32_t notePortsCount(bool isInput) const noexcept override 
    {
        return isInput ? 1 : 0;
    }
    
    bool stateSave(const clap_ostream *ostream) noexcept override;
    bool stateLoad(const clap_istream *stream) noexcept override;
    
    bool implementsState() const noexcept override{
        return true;
    }
    
    bool implementsParams() const noexcept override{
        return true;
    }
    
    bool implementsNotePorts() const noexcept override{     return true;    }
    bool implementsAudioPorts() const noexcept override{    return true;    }
    
//    bool isValidParamId(clap_id paramId) const noexcept override
//    {
//        return paramId == gainPrmId_;
//    }

    bool paramsValue(clap_id paramId, double* value) noexcept override;
    bool paramsValueToText(clap_id paramId, double value, char* display, uint32_t size) noexcept override;
    bool paramsTextToValue(clap_id paramId, const char* display, double* value) noexcept override;
    bool activate(double sampleRate, uint32_t, uint32_t) noexcept override;

    clap_process_status process(const clap_process* process) noexcept override;

    // Add GUI implementation
    bool implementsGui() const noexcept override { return true; }
    bool guiIsApiSupported(const char* api, bool isFloating) noexcept override;
    bool guiCreate(const char* api, bool isFloating) noexcept override;
    void guiDestroy() noexcept override;
    bool guiSetScale(double scale) noexcept override;
    bool guiSetSize(uint32_t width, uint32_t height) noexcept override;
    bool guiGetSize(uint32_t* width, uint32_t* height) noexcept override;
    bool guiShow() noexcept override;
    bool guiHide() noexcept override;
    bool guiSetParent(const clap_window* window) noexcept override;

private:
    std::string htmlMntPoint = {};
    lattice::Server server;
    // Add GUI members
    std::unique_ptr<choc::ui::WebView> webview;
    uint32_t currentWidth = 800;  // Default width
    uint32_t currentHeight = 600; // Default height

    // Utility functions for parameter handling
    void sendParameterValueToHost(clap_id paramId, double value) noexcept;
    void beginParamAdjust(clap_id paramId) noexcept;
    void endParamAdjust(clap_id paramId) noexcept;

    lattice::Processor& processor; // reference to processor
};

class LatticeProcessorPluginFactory {
public:
    static LatticeClapPlugin* createPlugin(const clap_host* host);
};
