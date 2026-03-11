#pragma once

#include "../LatticeStructs.h"
#include "../LatticeUtils.h"
#include <deque>

#include "choc/platform/choc_DisableAllWarnings.h"
#include <clap/ext/gui.h>
#include <clap/ext/timer-support.h>
#include <clap/helpers/host-proxy.hxx>
#include <clap/helpers/plugin.hxx>

#if __has_include(<ARA_API/ARACLAP.h>)
#include <ARA_API/ARACLAP.h>
#define LATTICE_HAS_ARA_CLAP 1
#else
#define LATTICE_HAS_ARA_CLAP 0
#endif

#include "choc/platform/choc_ReenableAllWarnings.h"
#include <readerwriterqueue.h>
#if !defined(LATTICE_LINUX)
#include "choc/gui/choc_WebView.h"
#include <clap/events.h>
#include <clap/ext/params.h>
#else
#include "../LatticeMemoryQueue.h"
#include "choc/text/choc_Files.h"
#endif

// Forward declare lattice::Processor
// and LatticeClapPlugin
class LatticeClapPlugin;

namespace lattice {
class Processor;
}

using pluginType = LatticeClapPlugin;

class LatticeClapPlugin
    : public clap::helpers::Plugin<clap::helpers::MisbehaviourHandler::Ignore,
                                   clap::helpers::CheckingLevel::Maximal> {

public:
  LatticeClapPlugin(const clap_host *host, lattice::Processor &processor);
  ~LatticeClapPlugin() override;

  bool audioPortsInfo(uint32_t index, bool isInput,
                      clap_audio_port_info *info) const noexcept override;
  bool paramsInfo(uint32_t paramIndex,
                  clap_param_info *info) const noexcept override;
  bool notePortsInfo(uint32_t index, bool isInput,
                     clap_note_port_info *info) const noexcept override;

  uint32_t paramsCount() const noexcept override;
  uint32_t audioPortsCount(bool /*isInput*/) const noexcept override;

  uint32_t notePortsCount(bool /*isInput*/) const noexcept override {
    return 1; // Both input and output note ports
  }

  bool stateSave(const clap_ostream *ostream) noexcept override;
  bool stateLoad(const clap_istream *stream) noexcept override;

  bool implementsState() const noexcept override { return true; }

  bool implementsParams() const noexcept override { return true; }

  bool implementsNotePorts() const noexcept override { return true; }
  bool implementsAudioPorts() const noexcept override { return true; }

  bool paramsValue(clap_id paramId, double *value) noexcept override;
  bool paramsValueToText(clap_id paramId, double value, char *display,
                         uint32_t size) noexcept override;
  bool paramsTextToValue(clap_id paramId, const char *display,
                         double *value) noexcept override;
  bool activate(double sampleRate, uint32_t, uint32_t) noexcept override;
  const void *extension(const char *id) noexcept override;

  // --- Helper functions ---
  void emitGestureBegin(clap_id paramId, const clap_output_events_t *outEvents);
  void emitGestureEnd(clap_id paramId, const clap_output_events_t *outEvents);
  void emitValue(clap_id paramId, double value,
                 const clap_output_events_t *outEvents);

  clap_process_status process(const clap_process *process) noexcept override;

  // Add GUI implementation
  bool implementsGui() const noexcept override { return true; }
  bool guiIsApiSupported(const char *api, bool isFloating) noexcept override;
  bool guiCreate(const char *api, bool isFloating) noexcept override;
  void guiDestroy() noexcept override;
  bool guiSetScale(double scale) noexcept override;
  bool guiSetSize(uint32_t width, uint32_t height) noexcept override;
  bool guiGetSize(uint32_t *width, uint32_t *height) noexcept override;
  bool guiShow() noexcept override;
  bool guiHide() noexcept override;
  bool guiSetParent(const clap_window *window) noexcept override;

  // Timer support extension
  bool implementsTimerSupport() const noexcept override { return true; }
  void onTimer(clap_id timerId) noexcept override;

  // Main thread callback (fallback when timer is not supported)
  void onMainThread() noexcept override;

  // Keyboard passthrough control
  // By default, all keypresses pass through to the DAW
  // Call setConsumeKeypresses(true) when the webview needs keyboard input
  // (e.g., text fields)
  void setConsumeKeypresses(bool consume) {
    consumeKeypresses.store(consume, std::memory_order_relaxed);
  }

  bool getConsumeKeypresses() const {
    return consumeKeypresses.load(std::memory_order_relaxed);
  }

  moodycamel::ReaderWriterQueue<lattice::ParameterChange> parameterChanges;
  moodycamel::ReaderWriterQueue<lattice::OutputNoteEvent> outputNoteEvents;
  moodycamel::ReaderWriterQueue<lattice::RawMidiEvent> rawMidiEvents;

#if LATTICE_HAS_ARA_CLAP
  bool hasAraSupport() const noexcept;
  const ARA::ARAFactory *getAraFactory() const noexcept;
  const ARA::ARAPlugInExtensionInstance *bindToAraDocumentController(
      ARA::ARADocumentControllerRef documentControllerRef,
      ARA::ARAPlugInInstanceRoleFlags knownRoles,
      ARA::ARAPlugInInstanceRoleFlags assignedRoles) const noexcept;
#endif

private:
  lattice::Processor &processor; // reference to processor
  clap_id timerId = CLAP_INVALID_ID;
  bool usingHostTimer = false; // Track if CLAP timer extension is active
  bool usingCallbackFallback =
      false; // Track if using request_callback fallback
  std::string htmlMntPoint = {};

  // Add GUI members
#ifdef LATTICE_LINUX
  void *webview = nullptr;
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

  // Keyboard handling
  // By default false - keys pass through to DAW via sendKeyEventToHost JS
  // binding Set to true when webview needs keyboard input (text fields, etc.)
  std::atomic<bool> consumeKeypresses{false};
#if LATTICE_WINDOWS
  HWND parentWindow = nullptr;
#endif
};

class LatticeProcessorPluginFactory {
public:
  static LatticeClapPlugin *createPlugin(const clap_host *host);
};
