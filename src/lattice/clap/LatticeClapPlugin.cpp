#include "LatticeClapPlugin.h"
#include "../LatticeProcessor.h"
#include <nlohmann/json.hpp>
#include PLUGIN_INFO_HEADER  // Include the dynamically generated header

#if LATTICE_WINDOWS
#include <windows.h>
#elif LATTICE_MACOS
extern "C"
{
    bool attachViewToParent(void* childView, void* parentView); // Forward declaration
}
#elif LATTICE_LINUX
#include <X11/Xlib.h>
#include "../LinuxWebviewProcess/webview_binary.h"
#endif


LatticeClapPlugin::LatticeClapPlugin(const clap_host* host, lattice::Processor& processor)
    : clap::helpers::Plugin<clap::helpers::MisbehaviourHandler::Ignore, clap::helpers::CheckingLevel::Maximal>(
        &descriptor, host), processor(processor)
#if LATTICE_LINUX
    , instanceMap(lattice::SharedMemoryQueue::CreateDefaultInstanceTracker(true)),
    memoryQueue("/lattice_" + instanceMap.getInstanceId(), 100, 1024)
#endif
{

    auto rootPath = processor.getMountPoint().empty() ? lattice::File::getResourceDirFromBundle() : processor.getMountPoint();

    if (!server.isThreadRunning())
        server.start(rootPath);

    htmlMntPoint = "http://127.0.0.1:" + std::to_string(server.getCurrentPort()) + "/index.html";
    processor.setServerUrl(htmlMntPoint);

    auto functionName = processor.getWebViewSendFunctionName();
    processor.sendWebViewMessage = [this, functionName](const std::string& script) {
#ifdef LATTICE_LINUX

#else
        if (webview)
        {
            std::stringstream fullScript;
            // Wrap call with function name
            fullScript << functionName << "(" << script << ")";
            webviewMessageQueue.enqueue(fullScript.str());
        }
        else
        {
            lattice::logDebug << "Messages are being sent to webview before it is open...";
        }
#endif
    };


    processor.addParameterChange = [this](lattice::ParameterChange param) {
        parameterChanges.enqueue(param);
    };



#ifdef LATTICE_LINUX
    webviewProcessPath = createTempFile(std::string("/tmp/latWV_" + instanceMap.getInstanceId() + "XXXXXX").c_str());
#endif
}

LatticeClapPlugin::~LatticeClapPlugin()
{
#ifdef LATTICE_LINUX
    // Terminate the webview process
    unlink(std::string(webviewProcessPath).c_str());
#endif
}

uint32_t LatticeClapPlugin::audioPortsCount(bool isInput) const noexcept
{
    if (isInput)
        return static_cast<uint32_t>(processor.getChannelConfig().getNumInputBuses());
    else
        return static_cast<uint32_t>(processor.getChannelConfig().getNumOutputBuses());

}

bool LatticeClapPlugin::audioPortsInfo(uint32_t index, bool isInput, clap_audio_port_info* info) const noexcept
{
    if (isInput)
    {
        if (static_cast<size_t>(index) >= processor.getChannelConfig().getNumInputBuses())
            return false;

        info->channel_count = processor.getChannelConfig().getNumInputChannels(index);
        snprintf(info->name, sizeof(info->name) - 1, "%s", processor.getChannelConfig().getInputBusName(index).c_str());
    }
    else
    {
        if (static_cast<size_t>(index) >= processor.getChannelConfig().getNumOutputBuses())
            return false;

        info->channel_count = processor.getChannelConfig().getNumOutputChannels(index);
        snprintf(info->name, sizeof(info->name) - 1, "%s", processor.getChannelConfig().getOutputBusName(index).c_str());
    }


    info->id = index;

    //use unique buffers for input/output
    info->in_place_pair = CLAP_INVALID_ID;

    if (index == 0)
        info->flags = CLAP_AUDIO_PORT_IS_MAIN;
    else
        info->flags = 0;

    info->port_type = CLAP_PORT_STEREO;

    return true;
}

bool LatticeClapPlugin::stateSave(const clap_ostream* ostream) noexcept
{
    auto json = processor.savePluginState();
    auto res = ostream->write(ostream, json.dump().data(), json.dump().size());
    return (res == -1 ? false : true);
}

bool LatticeClapPlugin::stateLoad(const clap_istream* istream) noexcept
{

    std::vector<char> buffer;
    const int64_t chunkSize = 2048; // Read in chunks

    while (true) {
        // Reserve space for the next chunk
        size_t old_size = buffer.size();
        buffer.resize(old_size + chunkSize);

        // Read the next chunk
        int64_t bytes_read = istream->read(istream, buffer.data() + old_size, chunkSize);

        if (bytes_read < 0)
            return false; // Error reading from the stream

        // Resize the buffer to the number of bytes read
        buffer.resize(old_size + bytes_read);

        if (bytes_read < chunkSize)
            break;

    }

    // Parse the JSON data
    try
    {
        std::string json_str(buffer.begin(), buffer.end());
        nlohmann::json json = nlohmann::json::parse(json_str); // Parse JSON string
        processor.loadPluginState(json); // Load the plugin state from the JSON object
        return true;
    }
    catch (const std::exception& e)
    {
        // Handle JSON parsing errors
        std::cerr << "Failed to parse JSON: " << e.what() << std::endl;
        return false;
    }
}

bool LatticeClapPlugin::paramsInfo(uint32_t paramId, clap_param_info* info) const noexcept
{
    auto numParameters = processor.getParameters().size();

    if (paramId >= numParameters)
        return false;


    const auto p = processor.getParameters()[paramId];

    info->id = paramId;
    info->flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_MODULATABLE;
    strncpy(info->name, p.name.c_str(), CLAP_NAME_SIZE);
    strncpy(info->module, "", CLAP_NAME_SIZE);
    info->min_value = 0.f;
    info->max_value = 1.f;
    info->default_value = 0.f;//p.value;//utils::decibelsToGain(0.0);


    return true;
}

bool LatticeClapPlugin::notePortsInfo(uint32_t index, bool isInput, clap_note_port_info* info) const noexcept
{
    if (!isInput || index)
        return false;
    info->id = 0;
    info->supported_dialects = CLAP_NOTE_DIALECT_MIDI | CLAP_NOTE_DIALECT_MIDI_MPE | CLAP_NOTE_DIALECT_CLAP;
    info->preferred_dialect = CLAP_NOTE_DIALECT_CLAP;
    snprintf(info->name, sizeof(info->name), "%s", "Note Port");

    return true;
}

bool LatticeClapPlugin::paramsValue(clap_id paramId, double* value) noexcept
{
    auto numParameters = processor.getParameters().size();

    if (paramId > numParameters)
        return false;

    *value = processor.getParameters()[paramId].value;
    return true;
}

bool LatticeClapPlugin::paramsValueToText(clap_id paramId, double value, char* display, uint32_t size) noexcept
{
    auto numParameters = processor.getParameters().size();

    if (paramId > numParameters)
        return false;

    const auto updatedValue = processor.getParameter(paramId).fromNormalised(value);

    //    processor.setParameter(paramId, updatedValue);


    snprintf(display, size, "%.2f", updatedValue);
    std::cout << display << std::endl;

    return true;
}

bool LatticeClapPlugin::paramsTextToValue(clap_id paramId, const char* display, double* value) noexcept
{
    auto numParameters = processor.getParameters().size();

    if (paramId > numParameters)
        return false;

    const double value_ = strtod(display, nullptr);

    *value = (value_);

    return true;
}

uint32_t LatticeClapPlugin::paramsCount() const noexcept
{
    return static_cast<uint32_t>(processor.getParameters().size());
}

bool LatticeClapPlugin::activate(double sampleRate, uint32_t minFrameCount, uint32_t maxFrameCount) noexcept
{
    processor.prepareToPlay(sampleRate, minFrameCount, maxFrameCount);
    return true;
}

clap_process_status LatticeClapPlugin::process(const clap_process* process) noexcept
{
    if (process->audio_outputs_count <= 0)
        return CLAP_PROCESS_CONTINUE;

    float** outputs = process->audio_outputs[0].data32;
    std::size_t blockSize = process->frames_count;

    // If there are inputs...
    if (process->audio_inputs)
    {
        float** inputs = process->audio_inputs[0].data32;
        processor.process(inputs, outputs, blockSize);
    }
    else
    {
        processor.process(nullptr, outputs, blockSize);
    }

    // Handle parameter changes
    auto event = process->in_events;
    for (uint32_t i = 0; i < event->size(event); ++i)
    {
        auto nextEvent = event->get(event, i);
        if (nextEvent->space_id == CLAP_CORE_EVENT_SPACE_ID &&
            nextEvent->type == CLAP_EVENT_PARAM_VALUE)
        {
            auto p = reinterpret_cast<const clap_event_param_value*>(nextEvent);
            if (p->param_id < processor.getParameters().size())
            {
                nlohmann::json j, h;
                j["command"] = "parameterChange";
                h["paramIdx"] = p->param_id;
                h["value"] = processor.getParameter(p->param_id).fromNormalised(p->value);
                j["data"] = h;

                std::stringstream fullScript;
                // Wrap call with function name
                auto functionName = processor.getWebViewSendFunctionName();
                fullScript << functionName << "(" << j.dump() << ")";
#ifdef LATTICE_LINUX

#else
                if (webview)
                {
                    webviewMessageQueue.enqueue(fullScript.str());
                }
#endif
                sendParameterValueToHost(p->param_id, p->value);

            }
        }
        else if (nextEvent->type == CLAP_EVENT_NOTE_ON || nextEvent->type == CLAP_EVENT_NOTE_OFF || nextEvent->type == CLAP_EVENT_NOTE_CHOKE) {
            const clap_event_note_t* noteEvent = (const clap_event_note_t*)nextEvent;

            // Map CLAP event types to NoteEvent::Type
            lattice::NoteEvent::Type type = {};

            switch (nextEvent->type)
            {
            case CLAP_EVENT_NOTE_ON:
                type = lattice::NoteEvent::Type::noteOn;
                break;
            case CLAP_EVENT_NOTE_OFF:
                type = lattice::NoteEvent::Type::noteOff;
                break;
            case CLAP_EVENT_NOTE_CHOKE:
                type = lattice::NoteEvent::Type::noteChoke;
                break;
            default:
                // Handle unexpected event types (optional)
                std::cerr << "Unexpected CLAP note event type: " << nextEvent->type << std::endl;
                break;
            }

            processor.addNoteEvent({ type,
                    noteEvent->key,
                    noteEvent->velocity,
                    noteEvent->note_id,
                    noteEvent->header.time });
        }
        else if (nextEvent->type == CLAP_EVENT_MIDI) {
            std::cout << "MIDI Event" << std::endl;
        }
    }

    lattice::ParameterChange change;
    while (parameterChanges.try_dequeue(change)) {
        switch (change.type) {
        case lattice::ParamChangeType::GestureBegin:
            emitGestureBegin(change.paramId, process->out_events);
            break;

        case lattice::ParamChangeType::Value:
            emitValue(change.paramId, change.value, process->out_events);
            break;

        case lattice::ParamChangeType::GestureEnd:
            emitGestureEnd(change.paramId, process->out_events);
            break;

        case lattice::ParamChangeType::Complete:
            emitGestureBegin(change.paramId, process->out_events);
            emitValue(change.paramId, change.value, process->out_events);
            emitGestureEnd(change.paramId, process->out_events);
            break;
        }
    }
    return CLAP_PROCESS_CONTINUE;

}


void LatticeClapPlugin::emitGestureBegin(clap_id paramId, const clap_output_events_t* outEvents)
{
    clap_event_param_gesture ev = {};
    ev.header.size = sizeof(ev);
    ev.header.time = 0;
    ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    ev.header.type = CLAP_EVENT_PARAM_GESTURE_BEGIN;
    ev.header.flags = 0;
    ev.param_id = paramId;
    outEvents->try_push(outEvents, (const clap_event_header_t*)&ev);
}

void LatticeClapPlugin::emitGestureEnd(clap_id paramId, const clap_output_events_t* outEvents)
{
    clap_event_param_gesture ev = {};
    ev.header.size = sizeof(ev);
    ev.header.time = 0;
    ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    ev.header.type = CLAP_EVENT_PARAM_GESTURE_END;
    ev.header.flags = 0;
    ev.param_id = paramId;
    outEvents->try_push(outEvents, (const clap_event_header_t*)&ev);
}

void LatticeClapPlugin::emitValue(clap_id paramId, double value, const clap_output_events_t* outEvents)
{
    clap_event_param_value ev = {};
    ev.header.size = sizeof(ev);
    ev.header.time = 0;
    ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    ev.header.type = CLAP_EVENT_PARAM_VALUE;
    ev.header.flags = 0;
    ev.param_id = paramId;
    ev.value = value;
    ev.note_id = -1;
    ev.channel = -1;
    ev.key = -1;
    ev.port_index = -1;
    outEvents->try_push(outEvents, (const clap_event_header_t*)&ev);
}


bool LatticeClapPlugin::guiIsApiSupported(const char* api, bool /*isFloating*/) noexcept
{
    // We support embedded and floating windows
    return strcmp(api, CLAP_WINDOW_API_WIN32) == 0 ||
        strcmp(api, CLAP_WINDOW_API_COCOA) == 0 ||
        strcmp(api, CLAP_WINDOW_API_X11) == 0;
}


void LatticeClapPlugin::sendParameterValueToHost(clap_id paramId, double value) const noexcept
{
    //checkMainThread();

    if (auto* host = _host.host())
    {
        if (auto* params = (const clap_host_params*)host->get_extension(host, CLAP_EXT_PARAMS))
        {
            double currentValue = processor.getParameterValue(paramId); // Assuming you have a getter
            if (currentValue != value)
            { // Only update if the value has changed
                processor.setParameter(paramId, value); // Update the processor's state
                params->request_flush(host); // Request a flush
            }
        }
    }
}

void LatticeClapPlugin::startTimer()
{
    isTimerRunning = true;
    timer = choc::messageloop::Timer(16, [this]() {
        return this->timerCallback();
    });
}

void LatticeClapPlugin::stopTimer()
{
    isTimerRunning = false;
}


//========================================================================================
// Temporary file creation for Linux
//========================================================================================
std::string LatticeClapPlugin::createTempFile(const char* /*path*/)
{
#ifdef LATTICE_LINUX
    // Allocate memory for the temporary file name
    char* temp_filename = new char[strlen(patt) + 1]; // +1 for the null terminator
    std::strcpy(temp_filename, path);

    // Create a temporary file
    int fd = mkstemp(temp_filename); // Creates and opens the file
    if (fd == -1)
    {
        delete[] temp_filename; // Clean up the allocated memory
        throw std::runtime_error("Failed to create temporary file");
    }

    // Write binary data to the file
    std::string decoded_binary = lattice::Base64::decode(webview_binary);
    auto data_size = decoded_binary.size(); // Use the size of the string, not strlen
    if (write(fd, decoded_binary.data(), data_size) != static_cast<ssize_t>(data_size))
    {
        close(fd);
        unlink(temp_filename);  // Clean up
        delete[] temp_filename; // Clean up the allocated memory
        throw std::runtime_error("Failed to write to temporary file");
    }

    // Mark the file as executable
    if (chmod(temp_filename, S_IRWXU) == -1)
    { // Read, write, execute by owner
        close(fd);
        unlink(temp_filename);  // Clean up
        delete[] temp_filename; // Clean up the allocated memory
        throw std::runtime_error("Failed to make file executable");
    }

    // Close the file
    close(fd);

    // Save the full path
    std::string full_path(temp_filename);

    // Clean up the allocated memory
    delete[] temp_filename;

    return full_path;
#endif
    return "";

}

bool LatticeClapPlugin::guiCreate(const char* /*api*/, bool /*isFloating*/) noexcept
{
#ifdef LATTICE_LINUX
    guiSetSize(processor.getEditorWidth(), processor.getEditorHeight());
    return true;
#else
    guiSetSize(processor.getEditorWidth(), processor.getEditorHeight());

    try {

        startTimer();
        choc::ui::WebView::Options options;
        options.enableDebugMode = true;
        options.acceptsFirstMouseClick = true;

        webview = std::make_unique<choc::ui::WebView>(options);

        if (!webview)
            return false;

        // Add JavaScript interface for parameter control
        webview->bind("sendMessageFromUI", [this](const choc::value::ValueView& args) -> choc::value::Value {
            nlohmann::json j = nlohmann::json::parse(choc::json::toString(args));
            processor.onMessageFromWebView(j);
            return {};
        });

        webview->navigate(htmlMntPoint);
        return true;

    }
    catch (const std::exception& e) {
        std::cerr << "Exception in guiCreate: " << e.what() << std::endl;
        return false;
    }
    catch (...) {
        std::cerr << "Unknown exception in guiCreate" << std::endl;
        return false;
    }

#endif

}

void LatticeClapPlugin::guiDestroy() noexcept
{
#ifdef LATTICE_LINUX

    nlohmann::json message;
    message["command"] = "Closed";
    message["data"] = "";
    memoryQueue.sendToChild(message);
    usleep(50000);
#else
    webview.reset();
    stopTimer();
#endif
}

bool LatticeClapPlugin::guiSetScale(double) noexcept
{
    return true;
}

bool LatticeClapPlugin::guiSetSize(uint32_t width, uint32_t height) noexcept
{
    currentWidth = width;
    currentHeight = height;
#ifdef LATTICE_LINUX
    return true;
#else
    return webview != nullptr;
#endif
}

bool LatticeClapPlugin::guiGetSize(uint32_t* width, uint32_t* height) noexcept
{
    *width = currentWidth;
    *height = currentHeight;
    return true;
}

bool LatticeClapPlugin::guiShow() noexcept
{
#ifdef LATTICE_LINUX
    nlohmann::json message;
    message["command"] = "LoadUrl";
    message["data"] = htmlMntPoint;
    memoryQueue.sendToChild(message);
    usleep(50000);
    return true;
#else
    return webview != nullptr;
#endif
}

bool LatticeClapPlugin::guiHide() noexcept
{
#ifdef LATTICE_LINUX
    return true;
#else
    return webview != nullptr;
#endif
}

bool LatticeClapPlugin::guiSetParent(const clap_window* window) noexcept
{
    try
    {
#if LATTICE_WINDOWS
        if (!webview)
            return false;

        if (strcmp(window->api, CLAP_WINDOW_API_WIN32) == 0)
        {
            auto* child = static_cast<HWND>(webview->getViewHandle());
            auto* parent = static_cast<::HWND>(window->win32);

            RECT parentRect;
            ::GetClientRect(parent, &parentRect);
            int parentWidth = parentRect.right - parentRect.left;
            int parentHeight = parentRect.bottom - parentRect.top;
            ::SetWindowPos(child, NULL, 0, 0, parentWidth, parentHeight, SWP_NOZORDER | SWP_SHOWWINDOW | SWP_FRAMECHANGED);

            ::InvalidateRect(child, NULL, false);
            ::SetWindowLongPtrW(child, GWL_STYLE, WS_CHILD);
            ::SetParent(child, parent);
            ::ShowWindow(child, SW_SHOW);
            return true;
        }
#elif LATTICE_MACOS
        if (!webview)
            return false;

        if (strcmp(window->api, CLAP_WINDOW_API_COCOA) == 0)
        {
            void* parent = window->cocoa;
            void* child = webview->getViewHandle();
            bool result = attachViewToParent(child, parent);
            return result;
        }
#elif LATTICE_LINUX
        if (strcmp(window->api, CLAP_WINDOW_API_X11) == 0)
        {
            // Convert parameters to strings
            std::ostringstream x11WindowIdStr, xStr, yStr, widthStr, heightStr, scaleStr;
            x11WindowIdStr << reinterpret_cast<unsigned long>(window->x11);
            xStr << 0;
            yStr << 0;
            widthStr << processor.getEditorWidth();
            heightStr << processor.getEditorHeight();;
            scaleStr << 1;
            std::string isTransparentStr = "true";
            std::string enableDevToolsStr = "true";

            // Fork process
            webviewPid = fork();

            if (webviewPid == 0)
            {

                std::vector<std::string> stringArgs = { webviewProcessPath,
                                                       x11WindowIdStr.str(),
                                                       "/lattice_" + instanceMap.getInstanceId(),
                                                       xStr.str(),
                                                       yStr.str(),
                                                       widthStr.str(),
                                                       heightStr.str(),
                                                       scaleStr.str(),
                                                       isTransparentStr,
                                                       enableDevToolsStr };

                std::vector<const char*> args;
                for (const auto& arg : stringArgs)
                    args.push_back(arg.c_str());


                usleep(10000);
                args.push_back(nullptr); // Null terminator for exec
                lattice::logInfo << "Webview process Name:" << args[0];
                execv(args[0], const_cast<char* const*>(args.data()));
                perror(args[0]); // Print error if exec fails
                // now kill the process that started the webview...
                exit(1);
            }

            if (webviewPid < 0)
            {
                lattice::logDebug << "Fork failed";
                return false;
            }
            return true;
        }
#endif

        return false;
    }
    catch (const std::exception& e)
    {
        return false;
    }
}

bool LatticeClapPlugin::timerCallback()
{
    std::string script;

    while (webviewMessageQueue.try_dequeue(script))
    {
        webview->evaluateJavascript(script, [](const std::string& error, const choc::value::ValueView& result) {
            if (!error.empty())
            {
                lattice::logDebug << "JavaScript Error: " << error;
            }
        });
    }

    return isTimerRunning;
}
