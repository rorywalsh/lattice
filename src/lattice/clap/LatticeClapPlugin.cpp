#include "LatticeClapPlugin.h"
#include "../LatticeProcessor.h"
#include <nlohmann/json.hpp>
#include PLUGIN_INFO_HEADER // Include the dynamically generated header

#if LATTICE_WINDOWS
#include <iomanip>
#include <windows.h>
#elif LATTICE_MACOS
extern "C" {
bool attachViewToParent(void *childView,
                        void *parentView); // Forward declaration
bool resizeView(void *view, uint32_t width,
                uint32_t height); // Forward declaration
}
#elif LATTICE_LINUX
#include "../LinuxWebviewProcess/webview_binary.h"
#include <X11/Xlib.h>
#endif

#if LATTICE_HAS_ARA_CLAP
namespace {
const ARA::ARAFactory *lattice_ara_get_factory(const clap_plugin_t *plugin) {
  auto *self = static_cast<const LatticeClapPlugin *>(plugin->plugin_data);
  if (!self) {
    return nullptr;
  }
  return self->getAraFactory();
}

const ARA::ARAPlugInExtensionInstance *lattice_ara_bind_to_document_controller(
    const clap_plugin_t *plugin,
    ARA::ARADocumentControllerRef documentControllerRef,
    ARA::ARAPlugInInstanceRoleFlags knownRoles,
    ARA::ARAPlugInInstanceRoleFlags assignedRoles) {
  auto *self = static_cast<const LatticeClapPlugin *>(plugin->plugin_data);
  if (!self) {
    return nullptr;
  }

  return self->bindToAraDocumentController(documentControllerRef, knownRoles,
                                           assignedRoles);
}

const clap_ara_plugin_extension_t lattice_ara_plugin_extension = {
    .get_factory = lattice_ara_get_factory,
    .bind_to_document_controller = lattice_ara_bind_to_document_controller,
};
} // namespace
#endif

LatticeClapPlugin::LatticeClapPlugin(const clap_host *host,
                                     lattice::Processor &processor)
    : clap::helpers::Plugin<clap::helpers::MisbehaviourHandler::Ignore,
                            clap::helpers::CheckingLevel::Maximal>(
          getDescriptor(), host),
      processor(processor)
#if LATTICE_LINUX
      ,
      instanceMap(
          lattice::SharedMemoryQueue::CreateDefaultInstanceTracker(true)),
      memoryQueue("/lattice_" + instanceMap.getInstanceId(), 100, 65536)
#endif
{

  lattice::logDebug << "Logger initialized and ready!";

  if (processor.getMountPoint().empty()) {
    const auto binaryPath = lattice::File::getBinaryFileAndPath();
    const auto binaryDir = lattice::File::getParentDirectory(binaryPath);
    const auto binaryName = lattice::File::getBinaryFileName();
    const auto binaryStem = std::filesystem::path(binaryName).stem().string();

    const auto candidateA = lattice::File::joinPath(binaryDir, "Resources");
    const auto candidateB =
        lattice::File::joinPath(binaryDir, binaryName, "Contents", "Resources");
    const auto candidateC =
        lattice::File::joinPath(binaryDir, binaryStem, "Contents", "Resources");
    const auto candidateD = lattice::File::joinPath(
        binaryDir, binaryName + "_resources", "Contents", "Resources");

    auto hasIndex = [](const std::string &dir) {
      return lattice::File::directoryExists(dir) &&
             lattice::File::exists(lattice::File::joinPath(dir, "index.html"));
    };

    if (hasIndex(candidateA)) {
      processor.setMountPoint(candidateA);
    } else if (hasIndex(candidateB)) {
      processor.setMountPoint(candidateB);
    } else if (hasIndex(candidateC)) {
      processor.setMountPoint(candidateC);
    } else if (hasIndex(candidateD)) {
      processor.setMountPoint(candidateD);
    }

    if (!processor.getMountPoint().empty()) {
      lattice::logDebug << "Auto-set mount point to: "
                        << processor.getMountPoint();
    }
  }

  auto functionName = processor.getWebViewSendFunctionName();
  processor.sendWebViewMessage = [this,
                                  functionName](const nlohmann::json &message) {
    std::stringstream fullScript;
    fullScript << "(function(m){"
               << "var fn='" << functionName << "';"
               << "if(typeof window[fn]==='function'){window[fn](m);}"
               << "else{window.__latticePendingMessages=window.__"
                  "latticePendingMessages||[];window.__latticePendingMessages."
                  "push({fn:fn,msg:m});}"
               << "})(" << message.dump() << ")";
    webviewMessageQueue.enqueue(fullScript.str());

    // Proactively request a main-thread callback so queued messages are
    // processed even when audio process/timer callbacks are intermittent.
    if (auto *host = _host.host()) {
      host->request_callback(host);
    }
  };

  processor.setWebViewHtml = [this](const std::string &html) {
#ifdef LATTICE_LINUX

#else
    if (webview) {
      webview->setHTML(html);
    } else {
      lattice::logDebug << "setWebViewHtml called but webview is null!";
    }
#endif
  };

  processor.addParameterChange = [this](lattice::ParameterChange param) {
    // Don't enqueue if we're shutting down to prevent race condition
    if (!isShuttingDown.load(std::memory_order_acquire)) {
      parameterChanges.enqueue(param);
    }
  };

  processor.addOutputNoteEvent = [this](lattice::OutputNoteEvent noteEvent) {
    // Don't enqueue if we're shutting down to prevent race condition
    if (!isShuttingDown.load(std::memory_order_acquire)) {
      outputNoteEvents.enqueue(noteEvent);
    }
  };

  processor.addRawMidiEvent = [this](lattice::RawMidiEvent midiEvent) {
    // Don't enqueue if we're shutting down to prevent race condition
    if (!isShuttingDown.load(std::memory_order_acquire)) {
      rawMidiEvents.enqueue(midiEvent);
    }
  };

  processor.requestGuiResize = [this](uint32_t width, uint32_t height) -> bool {
    if (auto *host = _host.host()) {
      if (auto *gui =
              (const clap_host_gui *)host->get_extension(host, CLAP_EXT_GUI)) {
        return gui->request_resize(host, width, height);
      }
    }
    return false;
  };

  processor.applyGuiResize = [this](uint32_t width, uint32_t height) {
    // Directly call guiSetSize to resize the webview
    // This is needed for hosts that accept resize but don't call guiSetSize
    // back
    guiSetSize(width, height);
  };

#ifdef LATTICE_LINUX
  webviewProcessPath = createTempFile(
      std::string("/tmp/latWV_" + instanceMap.getInstanceId() + "XXXXXX")
          .c_str());
#endif
}

LatticeClapPlugin::~LatticeClapPlugin() {
  // Set shutdown flag to prevent queue access from audio thread
  isShuttingDown.store(true, std::memory_order_release);

  // Give audio thread time to see the flag and exit any queue operations
  // Increased delay to ensure audio thread has exited any in-progress
  // operations
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  // Drain any remaining items from queues to prevent assertion on destruction
  lattice::ParameterChange paramDummy;
  while (parameterChanges.try_dequeue(paramDummy)) {
    // Just drain, don't process
  }
  lattice::OutputNoteEvent noteDummy;
  while (outputNoteEvents.try_dequeue(noteDummy)) {
    // Just drain, don't process
  }
  lattice::RawMidiEvent midiDummy;
  while (rawMidiEvents.try_dequeue(midiDummy)) {
    // Just drain, don't process
  }

#ifdef LATTICE_LINUX
  // Terminate the webview process
  unlink(std::string(webviewProcessPath).c_str());
#endif
}

uint32_t LatticeClapPlugin::audioPortsCount(bool isInput) const noexcept {
  if (isInput)
    return static_cast<uint32_t>(
        processor.getChannelConfig().getNumInputBuses());
  else
    return static_cast<uint32_t>(
        processor.getChannelConfig().getNumOutputBuses());
}

bool LatticeClapPlugin::audioPortsInfo(
    uint32_t index, bool isInput, clap_audio_port_info *info) const noexcept {
  if (isInput) {
    if (static_cast<size_t>(index) >=
        processor.getChannelConfig().getNumInputBuses())
      return false;

    info->channel_count =
        processor.getChannelConfig().getNumInputChannels(index);
    snprintf(info->name, sizeof(info->name) - 1, "%s",
             processor.getChannelConfig().getInputBusName(index).c_str());
  } else {
    if (static_cast<size_t>(index) >=
        processor.getChannelConfig().getNumOutputBuses())
      return false;

    info->channel_count =
        processor.getChannelConfig().getNumOutputChannels(index);
    snprintf(info->name, sizeof(info->name) - 1, "%s",
             processor.getChannelConfig().getOutputBusName(index).c_str());
  }

  info->id = index;

  // use unique buffers for input/output
  info->in_place_pair = CLAP_INVALID_ID;

  if (index == 0)
    info->flags = CLAP_AUDIO_PORT_IS_MAIN | CLAP_AUDIO_PORT_SUPPORTS_64BITS;
  else
    info->flags = CLAP_AUDIO_PORT_SUPPORTS_64BITS;

  // Map channel count to the appropriate CLAP port type string
  if (info->channel_count == 1)       info->port_type = CLAP_PORT_MONO;
  else if (info->channel_count == 2)  info->port_type = CLAP_PORT_STEREO;
  else                                info->port_type = nullptr;

  return true;
}

// ---------------------------------------------------------------------------
// Audio ports config extension
// ---------------------------------------------------------------------------

static const char* clapPortType(int channels) noexcept
{
    if (channels == 1) return CLAP_PORT_MONO;
    if (channels == 2) return CLAP_PORT_STEREO;
    return nullptr;
}

uint32_t LatticeClapPlugin::audioPortsConfigCount() const noexcept
{
    return static_cast<uint32_t>(processor.getChannelConfig().configs.size());
}

bool LatticeClapPlugin::audioPortsGetConfig(uint32_t index,
                                             clap_audio_ports_config *config) const noexcept
{
    const auto& configs = processor.getChannelConfig().configs;
    if (index >= configs.size()) return false;
    const auto& cfg = configs[index];

    config->id = cfg.id;
    snprintf(config->name, sizeof(config->name) - 1, "%s", cfg.name.c_str());
    config->input_port_count  = static_cast<uint32_t>(cfg.inputBuses.size());
    config->output_port_count = static_cast<uint32_t>(cfg.outputBuses.size());

    config->has_main_input = !cfg.inputBuses.empty();
    if (config->has_main_input)
    {
        config->main_input_channel_count = static_cast<uint32_t>(cfg.inputBuses[0].numChannels);
        config->main_input_port_type     = clapPortType(cfg.inputBuses[0].numChannels);
    }

    config->has_main_output = !cfg.outputBuses.empty();
    if (config->has_main_output)
    {
        config->main_output_channel_count = static_cast<uint32_t>(cfg.outputBuses[0].numChannels);
        config->main_output_port_type     = clapPortType(cfg.outputBuses[0].numChannels);
    }

    return true;
}

bool LatticeClapPlugin::audioPortsSetConfig(clap_id configId) noexcept
{
    processor.selectAudioPortsConfig(static_cast<uint32_t>(configId));
    return true;
}

bool LatticeClapPlugin::stateSave(const clap_ostream *ostream) noexcept {
  auto json = processor.savePluginState();
  auto res = ostream->write(ostream, json.dump().data(), json.dump().size());
  return (res == -1 ? false : true);
}

bool LatticeClapPlugin::stateLoad(const clap_istream *istream) noexcept {

  std::vector<char> buffer;
  const int64_t chunkSize = 2048; // Read in chunks

  while (true) {
    // Reserve space for the next chunk
    size_t old_size = buffer.size();
    buffer.resize(old_size + chunkSize);

    // Read the next chunk
    int64_t bytes_read =
        istream->read(istream, buffer.data() + old_size, chunkSize);

    if (bytes_read < 0)
      return false; // Error reading from the stream

    // Resize the buffer to the number of bytes read
    buffer.resize(old_size + bytes_read);

    if (bytes_read < chunkSize)
      break;
  }

  // Parse the JSON data
  try {
    std::string json_str(buffer.begin(), buffer.end());
    nlohmann::json json = nlohmann::json::parse(json_str); // Parse JSON string
    processor.loadPluginState(
        json); // Load the plugin state from the JSON object
    return true;
  } catch (const std::exception &e) {
    // Handle JSON parsing errors
    std::cerr << "Failed to parse JSON: " << e.what() << std::endl;
    return false;
  }
}

bool LatticeClapPlugin::paramsInfo(uint32_t paramId,
                                   clap_param_info *info) const noexcept {
  auto numParameters = processor.getParameters().size();

  if (paramId >= numParameters)
    return false;

  const auto p = processor.getParameters()[paramId];

  info->id = paramId;
  info->flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_MODULATABLE;

  // Check if this parameter should be stepped (discrete values)
  // A parameter is stepped if increment >= 1.0, indicating integer steps
  if (p.increment >= 1.0f) {
    info->flags |= CLAP_PARAM_IS_STEPPED;
  }

  strncpy(info->name, p.name.c_str(), CLAP_NAME_SIZE);
  strncpy(info->module, "", CLAP_NAME_SIZE);

  // Report actual parameter range to host (not normalized 0-1)
  info->min_value = p.min;
  info->max_value = p.max;

  // Calculate default value from the parameter's range
  // If no default is set, use the parameter's current value or min
  info->default_value = p.fromNormalised(0.5f); // Default to midpoint

  return true;
}

bool LatticeClapPlugin::notePortsInfo(
    uint32_t index, bool isInput, clap_note_port_info *info) const noexcept {
  if (index != 0)
    return false;

  info->id = isInput ? 0 : 1; // Different IDs for input and output ports
  info->supported_dialects = CLAP_NOTE_DIALECT_MIDI |
                             CLAP_NOTE_DIALECT_MIDI_MPE |
                             CLAP_NOTE_DIALECT_CLAP;
  info->preferred_dialect = CLAP_NOTE_DIALECT_CLAP;
  snprintf(info->name, sizeof(info->name), "%s",
           isInput ? "Note Input" : "Note Output");

  return true;
}

bool LatticeClapPlugin::paramsValue(clap_id paramId, double *value) noexcept {
  auto numParameters = processor.getParameters().size();

  if (paramId > numParameters)
    return false;

  // Return the parameter's actual value in its defined range [min, max]
  // Note: Parameter.value should be stored denormalized (in actual range)
  *value = processor.getParameter(paramId).fromNormalised(
      processor.getParameters()[paramId].value);
  return true;
}

bool LatticeClapPlugin::paramsValueToText(clap_id paramId, double value,
                                          char *display,
                                          uint32_t size) noexcept {
  auto numParameters = processor.getParameters().size();

  if (paramId > numParameters)
    return false;

  // value is already in actual range (denormalized) from CLAP
  // Format it for display
  snprintf(display, size, "%.2f", value);

  return true;
}

bool LatticeClapPlugin::paramsTextToValue(clap_id paramId, const char *display,
                                          double *value) noexcept {
  auto numParameters = processor.getParameters().size();

  if (paramId > numParameters)
    return false;

  const double parsedValue = strtod(display, nullptr);

  *value = parsedValue;

  return true;
}

uint32_t LatticeClapPlugin::paramsCount() const noexcept {
  return static_cast<uint32_t>(processor.getParameters().size());
}

bool LatticeClapPlugin::activate(double sampleRate, uint32_t minFrameCount,
                                 uint32_t maxFrameCount) noexcept {
  processor.prepareToPlay(sampleRate, minFrameCount, maxFrameCount);
  return true;
}

const void *LatticeClapPlugin::extension(const char *id) noexcept {
#if LATTICE_HAS_ARA_CLAP
  if (!strcmp(id, CLAP_EXT_ARA_PLUGINEXTENSION) && hasAraSupport()) {
    return &lattice_ara_plugin_extension;
  }
#endif

  return clap::helpers::Plugin<
      clap::helpers::MisbehaviourHandler::Ignore,
      clap::helpers::CheckingLevel::Maximal>::extension(id);
}

#if LATTICE_HAS_ARA_CLAP
bool LatticeClapPlugin::hasAraSupport() const noexcept {
  return getAraFactory() != nullptr;
}

const ARA::ARAFactory *LatticeClapPlugin::getAraFactory() const noexcept {
  auto *factory = processor.getAraFactory();
  return reinterpret_cast<const ARA::ARAFactory *>(factory);
}

const ARA::ARAPlugInExtensionInstance *
LatticeClapPlugin::bindToAraDocumentController(
    ARA::ARADocumentControllerRef documentControllerRef,
    ARA::ARAPlugInInstanceRoleFlags knownRoles,
    ARA::ARAPlugInInstanceRoleFlags assignedRoles) const noexcept {
  auto *instance = processor.bindToAraDocumentController(
      reinterpret_cast<void *>(documentControllerRef),
      static_cast<uint32_t>(knownRoles), static_cast<uint32_t>(assignedRoles));
  return reinterpret_cast<const ARA::ARAPlugInExtensionInstance *>(instance);
}
#endif

clap_process_status
LatticeClapPlugin::process(const clap_process *process) noexcept {
  if (process->audio_outputs_count <= 0)
    return CLAP_PROCESS_CONTINUE;

  float **outputs = process->audio_outputs[0].data32;

  std::size_t blockSize = process->frames_count;

  // Update transport info from host
  if (process->transport) {
    lattice::TransportInfo transport;
    const clap_event_transport_t *t = process->transport;

    if (t->flags & CLAP_TRANSPORT_HAS_TEMPO) {
      transport.tempo = t->tempo;
      transport.tempoInc = t->tempo_inc;
    }

    if (t->flags & CLAP_TRANSPORT_HAS_BEATS_TIMELINE) {
      transport.songPosBeats = t->song_pos_beats;
      transport.barStart = t->bar_start;
      transport.barNumber = t->bar_number;
    }

    if (t->flags & CLAP_TRANSPORT_HAS_SECONDS_TIMELINE) {
      transport.songPosSeconds = t->song_pos_seconds;
    }

    if (t->flags & CLAP_TRANSPORT_HAS_TIME_SIGNATURE) {
      transport.timeSigNum = t->tsig_num;
      transport.timeSigDenom = t->tsig_denom;
    }

    if (t->flags & CLAP_TRANSPORT_IS_LOOP_ACTIVE) {
      transport.isLoopActive = true;
      transport.loopStartBeats = t->loop_start_beats;
      transport.loopEndBeats = t->loop_end_beats;
      transport.loopStartSeconds = t->loop_start_seconds;
      transport.loopEndSeconds = t->loop_end_seconds;
    }

    transport.isPlaying = (t->flags & CLAP_TRANSPORT_IS_PLAYING) != 0;
    transport.isRecording = (t->flags & CLAP_TRANSPORT_IS_RECORDING) != 0;

    processor.setTransportInfo(transport);
  }

  // If there are inputs...
  if (process->audio_inputs && process->audio_inputs_count > 0) {
    float **inputs = process->audio_inputs[0].data32;

    processor.process(inputs, outputs, blockSize);
  } else {
    processor.process(nullptr, outputs, blockSize);
  }

  // Handle parameter changes
  auto event = process->in_events;
  for (uint32_t i = 0; i < event->size(event); ++i) {
    auto nextEvent = event->get(event, i);
    if (nextEvent->space_id == CLAP_CORE_EVENT_SPACE_ID &&
        nextEvent->type == CLAP_EVENT_PARAM_VALUE) {
      auto p = reinterpret_cast<const clap_event_param_value *>(nextEvent);
      if (p->param_id < processor.getParameters().size()) {
        const auto& param = processor.getParameter(p->param_id);
        // Clamp to the declared parameter range before sending anywhere.
        // Hosts can send out-of-range values when a user types a number directly
        // into the DAW's parameter text field — the plugin receives a clamped
        // value via setParameter(), but the frontend must also see the clamped value.
        const double clampedValue = std::max(static_cast<double>(param.min),
                                             std::min(static_cast<double>(param.max), p->value));

        nlohmann::json j, h;
        j["command"] = "parameterChange";
        h["paramIdx"] = p->param_id;
        // p->value is in actual range (denormalized) since we report actual min/max to CLAP
        h["value"] = clampedValue;
        j["data"] = h;

        std::stringstream fullScript;
        // Wrap call with function name
        auto functionName = processor.getWebViewSendFunctionName();
        fullScript << "(function(m){"
                   << "var fn='" << functionName << "';"
                   << "if(typeof window[fn]==='function'){window[fn](m);}"
                   << "else{window.__latticePendingMessages=window.__"
                      "latticePendingMessages||[];window.__"
                      "latticePendingMessages.push({fn:fn,msg:m});}"
                   << "})(" << j.dump() << ")";
#ifdef LATTICE_LINUX

#else
        webviewMessageQueue.enqueue(fullScript.str());
#endif
        // Normalize the clamped value before sending to processor.setParameter()
        // since setParameter expects normalized values [0-1]
        double normalizedValue =
            param.toNormalised(clampedValue);
        sendParameterValueToHost(p->param_id, normalizedValue);
      }
    } else if (nextEvent->type == CLAP_EVENT_NOTE_ON ||
               nextEvent->type == CLAP_EVENT_NOTE_OFF ||
               nextEvent->type == CLAP_EVENT_NOTE_CHOKE) {
      const clap_event_note_t *noteEvent = (const clap_event_note_t *)nextEvent;

      // Map CLAP event types to NoteEvent::Type
      lattice::NoteEvent::Type type = {};

      switch (nextEvent->type) {
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
        std::cerr << "Unexpected CLAP note event type: " << nextEvent->type
                  << std::endl;
        break;
      }

      processor.addNoteEvent({type, noteEvent->key, noteEvent->velocity,
                              noteEvent->note_id, noteEvent->header.time});
    } else if (nextEvent->type == CLAP_EVENT_MIDI) {
      std::cout << "MIDI Event" << std::endl;
    }
  }

  // Check shutdown flag before accessing queue to prevent race condition during
  // destruction
  if (!isShuttingDown.load(std::memory_order_acquire)) {
    // Handle parameter changes
    lattice::ParameterChange change;
    while (parameterChanges.try_dequeue(change)) {
      // ParameterChange.value is always normalized [0-1]
      // Convert to actual range before sending to CLAP host
      double denormalizedValue =
          processor.getParameter(change.paramId).fromNormalised(change.value);

      switch (change.type) {
      case lattice::ParamChangeType::GestureBegin:
        emitGestureBegin(change.paramId, process->out_events);
        break;

      case lattice::ParamChangeType::Value:
        emitValue(change.paramId, denormalizedValue, process->out_events);
        break;

      case lattice::ParamChangeType::GestureEnd:
        emitGestureEnd(change.paramId, process->out_events);
        break;

      case lattice::ParamChangeType::Complete:
        emitGestureBegin(change.paramId, process->out_events);
        emitValue(change.paramId, denormalizedValue, process->out_events);
        emitGestureEnd(change.paramId, process->out_events);
        break;
      }
    }

    // Handle output note events (MIDI output)
    lattice::OutputNoteEvent noteOut;
    while (outputNoteEvents.try_dequeue(noteOut)) {
      clap_event_note_t ev = {};
      ev.header.size = sizeof(ev);
      ev.header.time = noteOut.sampleOffset; // Sample-accurate timing
      ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
      ev.header.flags = 0;
      ev.note_id = noteOut.noteId;
      ev.port_index = 0;
      ev.channel = noteOut.channel;
      ev.key = noteOut.key;
      ev.velocity = noteOut.velocity;

      switch (noteOut.type) {
      case lattice::NoteEventType::noteOn:
        ev.header.type = CLAP_EVENT_NOTE_ON;
        break;
      case lattice::NoteEventType::noteOff:
        ev.header.type = CLAP_EVENT_NOTE_OFF;
        break;
      case lattice::NoteEventType::noteChoke:
        ev.header.type = CLAP_EVENT_NOTE_CHOKE;
        break;
      default:
        continue; // Skip undefined event types
      }

      process->out_events->try_push(process->out_events, &ev.header);
    }

    // Handle raw MIDI output events (CC, program change, pitch bend, etc.)
    lattice::RawMidiEvent midiOut;
    while (rawMidiEvents.try_dequeue(midiOut)) {
      clap_event_midi_t ev = {};
      ev.header.size = sizeof(ev);
      ev.header.time = midiOut.sampleOffset; // Sample-accurate timing
      ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
      ev.header.type = CLAP_EVENT_MIDI;
      ev.header.flags = 0;
      ev.port_index = 0;
      ev.data[0] = midiOut.data[0];
      ev.data[1] = midiOut.data[1];
      ev.data[2] = midiOut.data[2];

      process->out_events->try_push(process->out_events, &ev.header);
    }
  }

  // Fallback for hosts that don't support timer extension (e.g., AUv2 wrapper)
  // Request a main thread callback if we have pending webview messages
  if (usingCallbackFallback && webviewMessageQueue.peek() != nullptr) {
    // Queue has messages, request callback to process them on main thread
    if (auto *host = _host.host()) {
      host->request_callback(host);
    }
  }

  return CLAP_PROCESS_CONTINUE;
}

void LatticeClapPlugin::emitGestureBegin(
    clap_id paramId, const clap_output_events_t *outEvents) {
  clap_event_param_gesture ev = {};
  ev.header.size = sizeof(ev);
  ev.header.time = 0;
  ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
  ev.header.type = CLAP_EVENT_PARAM_GESTURE_BEGIN;
  ev.header.flags = 0;
  ev.param_id = paramId;
  outEvents->try_push(outEvents, (const clap_event_header_t *)&ev);
}

void LatticeClapPlugin::emitGestureEnd(clap_id paramId,
                                       const clap_output_events_t *outEvents) {
  clap_event_param_gesture ev = {};
  ev.header.size = sizeof(ev);
  ev.header.time = 0;
  ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
  ev.header.type = CLAP_EVENT_PARAM_GESTURE_END;
  ev.header.flags = 0;
  ev.param_id = paramId;
  outEvents->try_push(outEvents, (const clap_event_header_t *)&ev);
}

void LatticeClapPlugin::emitValue(clap_id paramId, double value,
                                  const clap_output_events_t *outEvents) {
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
  outEvents->try_push(outEvents, (const clap_event_header_t *)&ev);
}

bool LatticeClapPlugin::guiIsApiSupported(const char *api,
                                          bool /*isFloating*/) noexcept {
  // We support embedded and floating windows
  return strcmp(api, CLAP_WINDOW_API_WIN32) == 0 ||
         strcmp(api, CLAP_WINDOW_API_COCOA) == 0 ||
         strcmp(api, CLAP_WINDOW_API_X11) == 0;
}

void LatticeClapPlugin::sendParameterValueToHost(clap_id paramId,
                                                 double value) const noexcept {
  // checkMainThread();

  if (auto *host = _host.host()) {
    if (auto *params = (const clap_host_params *)host->get_extension(
            host, CLAP_EXT_PARAMS)) {
      double currentValue =
          processor.getParameterValue(paramId); // Assuming you have a getter
      if (currentValue != value) { // Only update if the value has changed
        processor.setParameter(paramId, value); // Update the processor's state
        params->request_flush(host);            // Request a flush
      }
    }
  }
}

void LatticeClapPlugin::startTimer() {
  usingHostTimer = false;
  usingCallbackFallback = false;

  if (auto *host = _host.host()) {
    if (auto *timerSupport =
            (const clap_host_timer_support *)host->get_extension(
                host, CLAP_EXT_TIMER_SUPPORT)) {
      // Request timer callback every 16ms (~60fps)
      if (timerSupport->register_timer(host, 16, &timerId)) {
        usingHostTimer = true;
        lattice::logDebug << "Timer registered with ID: " << timerId;
      } else {
        lattice::logDebug << "Failed to register timer";
      }
    } else {
      lattice::logDebug << "Host does not support timer extension";
    }
  }

  // If timer registration failed, use callback fallback
  if (!usingHostTimer) {
    usingCallbackFallback = true;
    lattice::logDebug << "Using host callback fallback for webview messages";
  }
}

void LatticeClapPlugin::stopTimer() {
  if (timerId != CLAP_INVALID_ID) {
    if (auto *host = _host.host()) {
      if (auto *timerSupport =
              (const clap_host_timer_support *)host->get_extension(
                  host, CLAP_EXT_TIMER_SUPPORT)) {
        timerSupport->unregister_timer(host, timerId);
        timerId = CLAP_INVALID_ID;
      }
    }
  }

  usingHostTimer = false;
  usingCallbackFallback = false;
}

//========================================================================================
// Temporary file creation for Linux
//========================================================================================
std::string LatticeClapPlugin::createTempFile(const char *path) {
#ifdef LATTICE_LINUX
  // Allocate memory for the temporary file name
  char *temp_filename =
      new char[strlen(path) + 1]; // +1 for the null terminator
  std::strcpy(temp_filename, path);

  // Create a temporary file
  int fd = mkstemp(temp_filename); // Creates and opens the file
  if (fd == -1) {
    delete[] temp_filename; // Clean up the allocated memory
    throw std::runtime_error("Failed to create temporary file");
  }

  // Write binary data to the file
  std::string decoded_binary = lattice::Base64::decode(webview_binary);
  auto data_size =
      decoded_binary.size(); // Use the size of the string, not strlen
  if (write(fd, decoded_binary.data(), data_size) !=
      static_cast<ssize_t>(data_size)) {
    close(fd);
    unlink(temp_filename);  // Clean up
    delete[] temp_filename; // Clean up the allocated memory
    throw std::runtime_error("Failed to write to temporary file");
  }

  // Mark the file as executable
  if (chmod(temp_filename, S_IRWXU) == -1) { // Read, write, execute by owner
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

bool LatticeClapPlugin::guiCreate(const char * /*api*/,
                                  bool /*isFloating*/) noexcept {
#ifdef LATTICE_LINUX
  guiSetSize(processor.getEditorWidth(), processor.getEditorHeight());
  auto resourceDir = processor.getMountPoint().empty()
                         ? lattice::File::getResourceDirFromBundle()
                         : processor.getMountPoint();
  htmlMntPoint = "file://" + lattice::File::joinPath(resourceDir, "index.html");
  startTimer();
  return true;
#else
  guiSetSize(processor.getEditorWidth(), processor.getEditorHeight());

  try {

    startTimer();
    choc::ui::WebView::Options options;
    options.enableDebugMode = true;
    options.acceptsFirstMouseClick = true;
    options.webviewIsReady = [&](choc::ui::WebView &wv) {
      // Add JavaScript interface for parameter control
      wv.bind("sendMessageFromUI",
              [this](const choc::value::ValueView &args) -> choc::value::Value {
                nlohmann::json j =
                    nlohmann::json::parse(choc::json::toString(args));
                processor.onMessageFromWebView(j);
                // Return a success value to resolve the promise in JavaScript
                return choc::value::Value(true);
              });

      // Add JavaScript interface for keyboard passthrough control
      // Call from frontend: window.consumeKeypresses(true) to capture keys in
      // webview Call window.consumeKeypresses(false) to pass keys through to
      // DAW
      wv.bind("consumeKeypresses",
              [this](const choc::value::ValueView &args) -> choc::value::Value {
                if (args.isArray() && args.size() > 0) {
                  bool consume = args[0].getWithDefault<bool>(false);
                  setConsumeKeypresses(consume);
                }
                return choc::value::Value(true);
              });

#if LATTICE_WINDOWS
      // Forward key events from the webview to the DAW host window.
      // Called from JS: window.sendKeyEventToHost(msgType, keyCode)
      // where msgType is WM_KEYDOWN (0x100), WM_KEYUP (0x101), etc.
      // and keyCode is the Win32 virtual key code (matches e.keyCode in JS).
      wv.bind("sendKeyEventToHost",
              [this](const choc::value::ValueView &args) -> choc::value::Value {
                if (!consumeKeypresses.load(std::memory_order_relaxed) &&
                    parentWindow) {
                  if (args.isArray() && args.size() >= 2) {
                    UINT msgType =
                        static_cast<UINT>(args[0].getWithDefault<int64_t>(0));
                    WPARAM vkCode =
                        static_cast<WPARAM>(args[1].getWithDefault<int64_t>(0));
                    HWND target = ::GetAncestor(parentWindow, GA_ROOT);
                    if (!target)
                      target = parentWindow;
                    ::PostMessage(target, msgType, vkCode, 0);
                    // lattice::logDebug << "KeyboardHandler: forwarding
                    // keyCode=" << vkCode
                    //                   << " msg=0x" << std::hex << msgType
                    //                   << " to DAW window=" << (void*)target;
                  }
                }
                return choc::value::Value(true);
              });
#endif

      // Temporary override to handle synchronous calls before this->webview is
      // assigned
      auto originalSetHtml = processor.setWebViewHtml;
      processor.setWebViewHtml = [&](const std::string &html) {
        wv.setHTML(html);
      };

#if LATTICE_WINDOWS
      // Windows WebView2 limitation: fetchResource is not called for subsequent
      // resources when using navigate() with custom protocols or setHTML()
      //
      // This code inlines JavaScript modules to work around this limitation.
      //
      // BUNDLER COMPATIBILITY: This code is compatible with webpack, esbuild,
      // Rollup, etc. If using a bundler:
      //   - Configure it to inline all JavaScript into the HTML (no external
      //   .js files)
      //   - If the bundle has no import statements, this code safely passes it
      //   through unchanged
      //   - Avoid external <script src="bundle.js"> as those won't load due to
      //   the WebView2 limitation

      auto resourceDir = processor.getMountPoint().empty()
                             ? lattice::File::getResourceDirFromBundle()
                             : processor.getMountPoint();

      try {
        auto htmlPath = lattice::File::joinPath(resourceDir, "index.html");
        auto htmlContent = lattice::File::getFileAsString(htmlPath);

        // Find and process all <script type="module"> tags with imports
        size_t scriptStart = 0;
        while ((scriptStart = htmlContent.find("<script type=\"module\">",
                                               scriptStart)) !=
               std::string::npos) {
          size_t scriptContentStart =
              scriptStart + 22; // Length of "<script type=\"module\">"
          size_t scriptEnd = htmlContent.find("</script>", scriptContentStart);

          if (scriptEnd == std::string::npos)
            break;

          std::string scriptContent = htmlContent.substr(
              scriptContentStart, scriptEnd - scriptContentStart);

          // Find import statements - if loading bundled html, there should not
          size_t importPos = 0;
          std::string modifiedScript = scriptContent;

          while ((importPos = modifiedScript.find("import {", importPos)) !=
                 std::string::npos) {
            size_t fromPos = modifiedScript.find("} from '", importPos);
            if (fromPos == std::string::npos) {
              fromPos = modifiedScript.find("} from \"", importPos);
            }

            if (fromPos != std::string::npos) {
              size_t pathStart = fromPos + 8; // Length of "} from '"
              size_t pathEnd = modifiedScript.find("'", pathStart);
              if (pathEnd == std::string::npos) {
                pathEnd = modifiedScript.find("\"", pathStart);
              }

              if (pathEnd != std::string::npos) {
                std::string importPath =
                    modifiedScript.substr(pathStart, pathEnd - pathStart);

                // Remove ./ prefix if present
                if (importPath.substr(0, 2) == "./") {
                  importPath = importPath.substr(2);
                }

                // Read the imported file
                auto importFullPath =
                    lattice::File::joinPath(resourceDir, importPath);
                if (lattice::File::exists(importFullPath)) {
                  auto importedContent =
                      lattice::File::getFileAsString(importFullPath);

                  // Remove the import line
                  size_t importLineEnd = modifiedScript.find(";", pathEnd);
                  if (importLineEnd != std::string::npos) {
                    modifiedScript.erase(importPos,
                                         importLineEnd - importPos + 1);

                    // Insert the imported content at the top of the script
                    modifiedScript = importedContent + "\n" + modifiedScript;

                    lattice::logDebug << "Inlined module: " << importPath;
                  }
                }

                importPos = pathEnd;
              } else {
                break;
              }
            } else {
              break;
            }
          }

          // Replace the original script content with modified version
          htmlContent.replace(scriptContentStart,
                              scriptEnd - scriptContentStart, modifiedScript);

          scriptStart = scriptEnd;
        }

        wv.setHTML(htmlContent);
        lattice::logDebug
            << "Loaded HTML with inlined modules for Windows WebView2";
      } catch (const std::exception &e) {
        lattice::logDebug << "ERROR loading/inlining HTML: " << e.what();
        // Fallback to navigate
        wv.navigate("choc://app/");
      }
#else
      wv.navigate("choc://app/");
#endif

      processor.onWebViewIsReady();

      // Flush any queued messages immediately once page load completes.
      processWebviewMessages();

      // Restore original handler that uses this->webview
      processor.setWebViewHtml = originalSetHtml;
    };

    options.fetchResource = [this](const std::string &path) {
      choc::ui::WebView::Options::Resource resource;
      auto resourceDir = processor.getMountPoint().empty()
                             ? lattice::File::getResourceDirFromBundle()
                             : processor.getMountPoint();

      if (path == "/" || path == "/index.html") {
        auto res = lattice::File::getFileAsString(
            lattice::File::joinPath(resourceDir, "index.html"));
        resource.data = std::vector<uint8_t>(res.begin(), res.end());
        resource.mimeType = "text/html";
        return resource;
      }

      // Clean the path - remove leading slash if present
      std::string cleanPath = path;
      if (!cleanPath.empty() && cleanPath[0] == '/') {
        cleanPath = cleanPath.substr(1);
      }

      // Strip query parameters (e.g., cache-busting timestamps)
      size_t queryPos = cleanPath.find('?');
      if (queryPos != std::string::npos) {
        cleanPath = cleanPath.substr(0, queryPos);
      }

      auto fullPath = lattice::File::joinPath(resourceDir, cleanPath);

      // Check if file exists before trying to load it
      if (!lattice::File::exists(fullPath)) {
        lattice::logDebug << "Resource not found: " << fullPath;
        return choc::ui::WebView::Options::Resource{}; // Return empty for
                                                       // missing files
      }

      auto res = lattice::File::getFileAsString(fullPath);
      resource.data = std::vector<uint8_t>(res.begin(), res.end());
      resource.mimeType = lattice::File::getMimeType(cleanPath);
      return resource;
    };

    webview = std::make_unique<choc::ui::WebView>(options);

    if (!webview) {
      lattice::logDebug << "WebView is null";
      return false;
    }

    return true;

  } catch (const std::exception &e) {
    std::cerr << "Exception in guiCreate: " << e.what() << std::endl;
    return false;
  } catch (...) {
    std::cerr << "Unknown exception in guiCreate" << std::endl;
    return false;
  }

#endif
}

void LatticeClapPlugin::guiDestroy() noexcept {
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

bool LatticeClapPlugin::guiSetScale(double) noexcept { return true; }

bool LatticeClapPlugin::guiSetSize(uint32_t width, uint32_t height) noexcept {
  currentWidth = width;
  currentHeight = height;

#ifdef LATTICE_LINUX
  // TODO: Send resize message to Linux webview process
  return true;
#elif LATTICE_WINDOWS
  if (!webview)
    return false;

  // Resize the webview window to match the new size
  auto *child = static_cast<HWND>(webview->getViewHandle());
  if (child) {
    ::SetWindowPos(child, NULL, 0, 0, width, height,
                   SWP_NOZORDER | SWP_NOMOVE | SWP_FRAMECHANGED);
    ::InvalidateRect(child, NULL, false);

    // Trigger a resize event in JavaScript so the page knows to reflow
    std::string resizeScript = "window.dispatchEvent(new Event('resize'));";
    webview->evaluateJavascript(
        resizeScript,
        [](const std::string &error, const choc::value::ValueView &) {
          if (!error.empty()) {
            lattice::logDebug << "Resize event dispatch error: " << error;
          }
        });

    return true;
  }
  return false;
#elif LATTICE_MACOS
  if (!webview)
    return false;

  // Resize the webview's NSView using platform-specific helper
  void *viewHandle = webview->getViewHandle();
  if (viewHandle) {
    bool success = resizeView(viewHandle, width, height);
    if (success) {
      // Trigger a resize event in JavaScript so the page knows to reflow
      // This allows CSS and JavaScript to respond to the new viewport size
      std::string resizeScript = "window.dispatchEvent(new Event('resize'));";
      webview->evaluateJavascript(
          resizeScript,
          [](const std::string &error, const choc::value::ValueView &) {
            if (!error.empty()) {
              lattice::logDebug << "Resize event dispatch error: " << error;
            }
          });
    }
    return success;
  }
  return false;
#else
  return webview != nullptr;
#endif
}

bool LatticeClapPlugin::guiGetSize(uint32_t *width, uint32_t *height) noexcept {
  *width = currentWidth;
  *height = currentHeight;
  return true;
}

bool LatticeClapPlugin::guiShow() noexcept {
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

bool LatticeClapPlugin::guiHide() noexcept {
#ifdef LATTICE_LINUX
  return true;
#else
  return webview != nullptr;
#endif
}

bool LatticeClapPlugin::guiSetParent(const clap_window *window) noexcept {
  try {
#if LATTICE_WINDOWS
    if (!webview)
      return false;

    if (strcmp(window->api, CLAP_WINDOW_API_WIN32) == 0) {
      auto *child = static_cast<HWND>(webview->getViewHandle());
      auto *parent = static_cast<::HWND>(window->win32);

      RECT parentRect;
      ::GetClientRect(parent, &parentRect);
      int parentWidth = parentRect.right - parentRect.left;
      int parentHeight = parentRect.bottom - parentRect.top;
      ::SetWindowPos(child, NULL, 0, 0, parentWidth, parentHeight,
                     SWP_NOZORDER | SWP_SHOWWINDOW | SWP_FRAMECHANGED);

      ::InvalidateRect(child, NULL, false);
      ::SetWindowLongPtrW(child, GWL_STYLE, WS_CHILD);
      ::SetParent(child, parent);
      ::ShowWindow(child, SW_SHOW);

      // Store parent window for key forwarding via sendKeyEventToHost JS
      // binding
      parentWindow = parent;

      return true;
    }
#elif LATTICE_MACOS
    if (!webview)
      return false;

    if (strcmp(window->api, CLAP_WINDOW_API_COCOA) == 0) {
      void *parent = window->cocoa;
      void *child = webview->getViewHandle();
      bool result = attachViewToParent(child, parent);
      return result;
    }
#elif LATTICE_LINUX
    if (strcmp(window->api, CLAP_WINDOW_API_X11) == 0) {
      // Convert parameters to strings
      std::ostringstream x11WindowIdStr, xStr, yStr, widthStr, heightStr,
          scaleStr;
      x11WindowIdStr << reinterpret_cast<unsigned long>(window->x11);
      xStr << 0;
      yStr << 0;
      widthStr << processor.getEditorWidth();
      heightStr << processor.getEditorHeight();
      ;
      scaleStr << 1;
      std::string isTransparentStr = "true";
      std::string enableDevToolsStr = "true";

      // Fork process
      webviewPid = fork();

      if (webviewPid == 0) {

        std::vector<std::string> stringArgs = {webviewProcessPath,
                                               x11WindowIdStr.str(),
                                               "/lattice_" +
                                                   instanceMap.getInstanceId(),
                                               xStr.str(),
                                               yStr.str(),
                                               widthStr.str(),
                                               heightStr.str(),
                                               scaleStr.str(),
                                               isTransparentStr,
                                               enableDevToolsStr};

        std::vector<const char *> args;
        for (const auto &arg : stringArgs)
          args.push_back(arg.c_str());

        usleep(10000);
        args.push_back(nullptr); // Null terminator for exec
        lattice::logInfo << "Webview process Name:" << args[0];
        execv(args[0], const_cast<char *const *>(args.data()));
        perror(args[0]); // Print error if exec fails
        // now kill the process that started the webview...
        exit(1);
      }

      if (webviewPid < 0) {
        lattice::logDebug << "Fork failed";
        return false;
      }
      return true;
    }
#endif

    return false;
  } catch (const std::exception &e) {
    return false;
  }
}

void LatticeClapPlugin::onTimer(clap_id /*timerId*/) noexcept {
  // This is called on the main thread by the host
  processWebviewMessages();
}

void LatticeClapPlugin::onMainThread() noexcept {
  // This is called on the main thread by the host in response to
  // request_callback() Used as fallback when timer extension is not supported
  processWebviewMessages();
}

void LatticeClapPlugin::processWebviewMessages() {
#if !defined(LATTICE_LINUX)
  if (!webview)
    return;
#endif

#ifdef LATTICE_LINUX
  // Read messages sent from the webview JS (child → host direction)
  nlohmann::json incomingMsg;
  while (memoryQueue.receiveFromChild(incomingMsg)) {
    try {
      processor.onMessageFromWebView(incomingMsg);
    } catch (const std::exception &e) {
      lattice::logError << "Exception in onMessageFromWebView: " << e.what();
    } catch (...) {
      lattice::logError << "Unknown exception in onMessageFromWebView";
    }
  }
#endif

  std::string script;

  // Deduplicate messages by extracting paramIdx from parameterChange messages
  // This prevents lag from processing redundant updates during rapid parameter
  // changes
  std::unordered_map<int, std::string> latestParamChanges;
  std::vector<std::string> otherMessages;

  while (webviewMessageQueue.try_dequeue(script)) {
    // Try to parse as JSON to check if it's a parameterChange message
    try {
      // Extract the JSON part from the function call: functionName({"command":
      // ...})
      size_t openParen = script.find('(');
      size_t closeParen = script.rfind(')');

      if (openParen != std::string::npos && closeParen != std::string::npos &&
          closeParen > openParen) {
        std::string jsonPart =
            script.substr(openParen + 1, closeParen - openParen - 1);
        auto j = nlohmann::json::parse(jsonPart);

        // Check if this is a parameterChange message
        if (j.contains("command") && j["command"] == "parameterChange" &&
            j.contains("data") && j["data"].contains("paramIdx")) {
          int paramIdx = j["data"]["paramIdx"];
          // Keep only the latest message for each paramIdx
          latestParamChanges[paramIdx] = script;
        } else {
          // Not a parameterChange, process immediately
          otherMessages.push_back(script);
        }
      } else {
        // Can't parse, process immediately
        otherMessages.push_back(script);
      }
    } catch (...) {
      // Parse failed, process immediately
      otherMessages.push_back(script);
    }
  }

  // Process non-parameterChange messages first
  for (const auto &msg : otherMessages) {
#ifdef LATTICE_LINUX
    nlohmann::json linuxMsg;
    linuxMsg["command"] = "EvaluateJS";
    linuxMsg["data"] = msg;
    memoryQueue.sendToChild(linuxMsg);
#else
    webview->evaluateJavascript(
        msg, [](const std::string &error,
                const choc::value::ValueView & /*result*/) {
          if (!error.empty()) {
            lattice::logDebug << "JavaScript Error: " << error;
          }
        });
#endif
  }

  // Then process only the latest parameterChange for each parameter
  for (const auto &[paramIdx, msg] : latestParamChanges) {
#ifdef LATTICE_LINUX
    nlohmann::json linuxMsg;
    linuxMsg["command"] = "EvaluateJS";
    linuxMsg["data"] = msg;
    memoryQueue.sendToChild(linuxMsg);
#else
    webview->evaluateJavascript(
        msg, [](const std::string &error,
                const choc::value::ValueView & /*result*/) {
          if (!error.empty()) {
            lattice::logDebug << "JavaScript Error: " << error;
          }
        });
#endif
  }
}
