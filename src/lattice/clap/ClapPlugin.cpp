#include "ClapPlugin.h"
#include "gui/choc_WebView.h"
#include "../LatticeProcessor.h"
#include <nlohmann/json.hpp>

#define CABBAGE_MACOS 1

#if CABBAGE_WINDOWS
#include <windows.h>
#elif CABBAGE_MACOS
extern "C"
{
    bool attachViewToParent(void *childView, void *parentView); // Forward declaration
}
#elif CABBAGE_LINUX
#include <X11/Xlib.h>
#endif


ClapPlugin::ClapPlugin(const clap_host* host, lattice::Processor& processor)
: clap::helpers::Plugin<clap::helpers::MisbehaviourHandler::Ignore, clap::helpers::CheckingLevel::Maximal>(
    nullptr, host), processor(processor)
{

    auto rootPath = lattice::File::getResourceDir();
    
    if (!server.isThreadRunning())
        server.start(rootPath);
    
    htmlMntPoint = "http://127.0.0.1:" + std::to_string(server.getCurrentPort()) + "/index.html";
    
    processor.sendParameterUpdateToHost = [this](uint32_t paramId, float value) {
        sendParameterValueToHost(paramId, value);
    };
    
    processor.sendWebViewMessage = [this](nlohmann::json j) {
        if (webview)
        {
            webview->evaluateJavascript("hostMessageCallback(" +
                                        j.dump() + ");");
        }
    };
    
    
    
}

ClapPlugin::~ClapPlugin()
{
    
}


bool ClapPlugin::audioPortsInfo(uint32_t index, bool /*isInput*/, clap_audio_port_info* info) const noexcept
{
    if (index != 0)
        return false;


    info->id = 0;
    info->in_place_pair = CLAP_INVALID_ID;
    strncpy(info->name, "main", sizeof(info->name));
    info->flags = CLAP_AUDIO_PORT_IS_MAIN;
    info->channel_count = processor.getNumOutputs();
    info->port_type = CLAP_PORT_STEREO;

    return true;
}

bool ClapPlugin::paramsInfo(uint32_t paramId, clap_param_info* info) const noexcept
{
    auto numParameters = processor.getParameters().size();
    
    if (paramId >= numParameters)
        return false;


    const auto p = processor.getParameters()[paramId];

    info->id = paramId;
    info->flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_MODULATABLE;
    strncpy(info->name, p.name, CLAP_NAME_SIZE);
    strncpy(info->module, "", CLAP_NAME_SIZE);
    info->min_value = 0.f;
    info->max_value = 1.f;
    info->default_value = 0.f;//p.value;//utils::decibelsToGain(0.0);


    return true;
}

bool ClapPlugin::notePortsInfo(uint32_t index, bool isInput, clap_note_port_info *info) const noexcept
{
    if (!isInput || index) 
        return false;
    info->id = 0;
    info->supported_dialects = CLAP_NOTE_DIALECT_MIDI | CLAP_NOTE_DIALECT_MIDI_MPE | CLAP_NOTE_DIALECT_CLAP;
    info->preferred_dialect = CLAP_NOTE_DIALECT_CLAP;
    snprintf(info->name, sizeof(info->name), "%s", "Note Port");
    
    return true;
}

bool ClapPlugin::paramsValue(clap_id paramId, double* value) noexcept
{
    auto numParameters = processor.getParameters().size();
    
    if (paramId > numParameters)
        return false;

    *value = processor.getParameters()[paramId].value;
    return true;
}

bool ClapPlugin::paramsValueToText(clap_id paramId, double value, char* display, uint32_t size) noexcept
{
    auto numParameters = processor.getParameters().size();
    
    if (paramId > numParameters)
        return false;
    
    processor.setParameter(paramId, value);
    
    snprintf(display, size, "%.2f dB", value);
    std::cout << display << std::endl;
    
    return true;
}

bool ClapPlugin::paramsTextToValue(clap_id paramId, const char* display, double* value) noexcept
{
    auto numParameters = processor.getParameters().size();
    
    if (paramId > numParameters)
        return false;

    const double value_ = strtod(display, nullptr);

    *value = (value_);

    return true;
}

bool ClapPlugin::activate(double sampleRate, uint32_t minFrameCount, uint32_t maxFrameCount) noexcept
{
    processor.prepareToPlay(sampleRate, minFrameCount, maxFrameCount);
    return true;
}

clap_process_status ClapPlugin::process(const clap_process* process) noexcept 
{
    if (process->audio_outputs_count <= 0)
        return CLAP_PROCESS_CONTINUE;

    // Call the CabbageProcessor's process method
    float** inputs = process->audio_inputs[0].data32;
    float** outputs = process->audio_outputs[0].data32;
    std::size_t blockSize = process->frames_count;


    processor.process(inputs, outputs, blockSize);

    // Handle parameter changes
    auto event = process->in_events;
    for (uint32_t i = 0; i < event->size(event); ++i)
    {
     auto nextEvent = event->get(event, i);
     if (nextEvent->space_id == CLAP_CORE_EVENT_SPACE_ID &&
         nextEvent->type == CLAP_EVENT_PARAM_VALUE)
     {
         auto p = reinterpret_cast<const clap_event_param_value*>(nextEvent);
         if (p->param_id == 0)
         {
             if (webview)
             {
                 nlohmann::json j, h;
                 j["command"] = "parameterChange";
                 h["paramIdx"] = p->param_id;
                 h["value"] = p->value;
                 j["data"] = h;
                 
                 webview->evaluateJavascript("hostMessageCallback(" +
                                             j.dump() + ");");
                 
             }
         }
     }
     else if (nextEvent->type == CLAP_EVENT_NOTE_ON || nextEvent->type == CLAP_EVENT_NOTE_OFF || nextEvent->type == CLAP_EVENT_NOTE_CHOKE) {
         const clap_event_note_t *noteEvent = (const clap_event_note_t *) nextEvent;

         // Map CLAP event types to NoteEvent::Type
         lattice::Processor::NoteEvent::Type type;
         
         switch (nextEvent->type)
         {
             case CLAP_EVENT_NOTE_ON:
                 type = lattice::Processor::NoteEvent::Type::noteOn;
                 break;
             case CLAP_EVENT_NOTE_OFF:
                 type = lattice::Processor::NoteEvent::Type::noteOff;
                 break;
             case CLAP_EVENT_NOTE_CHOKE:
                 type = lattice::Processor::NoteEvent::Type::noteChoke;
                 break;
             default:
                 // Handle unexpected event types (optional)
                 std::cerr << "Unexpected CLAP note event type: " << nextEvent->type << std::endl;
                 return; // Skip this event
         }
         
         processor.addNoteEvent({type,
                 noteEvent->key,
                 noteEvent->velocity,
                 noteEvent->note_id,
                 noteEvent->header.time});
         }
     else if (nextEvent->type == CLAP_EVENT_MIDI){
         std::cout << "MIDI Event" << std::endl;
     }
    }

    return CLAP_PROCESS_CONTINUE;

}

bool ClapPlugin::guiIsApiSupported(const char* api, bool /*isFloating*/) noexcept
{
    // We support embedded and floating windows
    return strcmp(api, CLAP_WINDOW_API_WIN32) == 0 ||
           strcmp(api, CLAP_WINDOW_API_COCOA) == 0 ||
           strcmp(api, CLAP_WINDOW_API_X11) == 0;
}


// Utility function to send parameter changes to host
void ClapPlugin::sendParameterValueToHost(clap_id paramId, double value) noexcept {
    if (auto* host = _host.host()) {
        if (auto* params = (const clap_host_params*) host->get_extension(host, CLAP_EXT_PARAMS)) {
            params->request_flush(host);
            processor.setParameter(paramId, value);
        }
    }
}


//========================================================================================

bool ClapPlugin::guiCreate(const char* /*api*/, bool /*isFloating*/) noexcept
{
    guiSetSize(processor.getEditorWidth(), processor.getEditorHeight());

    try {
        choc::ui::WebView::Options options;
        options.enableDebugMode = true;
        
        webview = std::make_unique<choc::ui::WebView>(options);
        
        if (!webview)
            return false;

        // Add JavaScript interface for parameter control
        webview->bind("sendMessageFromUI", [this](const choc::value::ValueView& args) -> choc::value::Value {
            nlohmann::json j = nlohmann::json::parse(choc::json::toString(args));
            processor.onMesssgeFromWebView(j);
            return {};
        });

        webview->navigate(htmlMntPoint);
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception in guiCreate: " << e.what() << std::endl;
        return false;
    }
    catch (...) {
        std::cerr << "Unknown exception in guiCreate" << std::endl;
        return false;
    }
}

void ClapPlugin::guiDestroy() noexcept 
{
    webview.reset();
}

bool ClapPlugin::guiSetScale(double) noexcept 
{
    return true;
}

bool ClapPlugin::guiSetSize(uint32_t width, uint32_t height) noexcept 
{
    currentWidth = width;
    currentHeight = height;
    return webview != nullptr;
}

bool ClapPlugin::guiGetSize(uint32_t* width, uint32_t* height) noexcept 
{
    *width = currentWidth;
    *height = currentHeight;
    return true;
}

bool ClapPlugin::guiShow() noexcept 
{
    return webview != nullptr;
}

bool ClapPlugin::guiHide() noexcept 
{
    return webview != nullptr;
}

bool ClapPlugin::guiSetParent(const clap_window *window) noexcept
{
    if (!webview)
    {
        return false;
    }

    try
    {
#if CABBAGE_WINDOWS
        if (strcmp(window->api, CLAP_WINDOW_API_WIN32) == 0)
        {
            auto *child = static_cast<HWND>(webview->getViewHandle());
            auto *parent = static_cast<::HWND>(window->win32);
            ::InvalidateRect(child, NULL, false);
            ::SetWindowLongPtrW(child, GWL_STYLE, WS_CHILD);
            ::SetParent(child, parent);
            ::ShowWindow(child, SW_SHOW);
            return true;
        }
#elif CABBAGE_MACOS
        if (strcmp(window->api, CLAP_WINDOW_API_COCOA) == 0)
        {
            void *parent = window->cocoa;
            void *child = webview->getViewHandle();
            bool result = attachViewToParent(child, parent);
            return result;
        }
#elif CABBAGE_LINUX
        if (strcmp(window->api, CLAP_WINDOW_API_X11) == 0)
        {
            XReparentWindow(XOpenDisplay(nullptr), (Window)viewHandle, (Window)window->x11, 0, 0);
            return true;
        }
#endif

        return false;
    }
    catch (const std::exception &e)
    {
        return false;
    }
}
