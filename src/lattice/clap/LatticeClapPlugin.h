#pragma once

#include "../LatticeUtils.h"
#include "../LatticeStructs.h"
#include <deque>

#include "choc/platform/choc_DisableAllWarnings.h"
#include <clap/helpers/host-proxy.hxx>
#include <clap/helpers/plugin.hxx>
#include <clap/ext/timer-support.h>

#include <readerwriterqueue.h>
#include "choc/platform/choc_ReenableAllWarnings.h"
#if !defined(LATTICE_LINUX)
#include <clap/ext/params.h>
#include <clap/events.h>
#include "choc/gui/choc_WebView.h"
#else
#include "text/choc_Files.h"
#include "../LatticeMemoryQueue.h"
#endif



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

    uint32_t notePortsCount(bool /*isInput*/) const noexcept override {
        return 1;  // Both input and output note ports
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
    

    bool paramsValue(clap_id paramId, double* value) noexcept override;
    bool paramsValueToText(clap_id paramId, double value, char* display, uint32_t size) noexcept override;
    bool paramsTextToValue(clap_id paramId, const char* display, double* value) noexcept override;
    bool activate(double sampleRate, uint32_t, uint32_t) noexcept override;

    // --- Helper functions ---
    void emitGestureBegin(clap_id paramId, const clap_output_events_t* outEvents);
    void emitGestureEnd(clap_id paramId, const clap_output_events_t* outEvents);
    void emitValue(clap_id paramId, double value, const clap_output_events_t* outEvents);

    
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

    // Timer support extension
    bool implementsTimerSupport() const noexcept override { return true; }
    void onTimer(clap_id timerId) noexcept override;

    moodycamel::ReaderWriterQueue<lattice::ParameterChange> parameterChanges;
    moodycamel::ReaderWriterQueue<lattice::OutputNoteEvent> outputNoteEvents;
    moodycamel::ReaderWriterQueue<lattice::RawMidiEvent> rawMidiEvents;
private:
    lattice::Processor& processor; // reference to processor
    clap_id timerId = CLAP_INVALID_ID;
    std::string htmlMntPoint = {};

    // Add GUI members
#ifdef LATTICE_LINUX
    void* webview = nullptr;
    std::string webviewProcessPath = "";
    pid_t webviewPid = 1;
    lattice::SharedMemoryQueue instanceMap;
    lattice::SharedMemoryQueue memoryQueue;

#else
    std::unique_ptr<choc::ui::WebView> webview;
#endif

    // Create a file path to the Linux webview process
    static std::string createTempFile(const char *path_template);

    uint32_t currentWidth = 800;  // Default width
    uint32_t currentHeight = 600; // Default height

    // Utility functions for parameter handling
    void sendParameterValueToHost(clap_id paramId, double value) const noexcept;

    void startTimer();
    void stopTimer();
    void processWebviewMessages();

    moodycamel::ReaderWriterQueue<std::string> webviewMessageQueue;
    std::atomic<bool> isShuttingDown{false};
};

class LatticeProcessorPluginFactory {
public:
    static LatticeClapPlugin* createPlugin(const clap_host* host);
};
