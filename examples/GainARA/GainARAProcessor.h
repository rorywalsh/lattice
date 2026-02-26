#pragma once

#include "lattice/LatticeProcessor.h"
#include <atomic>
#include <deque>
#include <mutex>

#if __has_include(<ARA_API/ARACLAP.h>) && __has_include(<ARA_Library/PlugIn/ARAPlug.h>)
#include <ARA_API/ARACLAP.h>
#include <ARA_Library/PlugIn/ARAPlug.h>
#define LATTICE_GAINARA_HAS_ARA 1
#else
#define LATTICE_GAINARA_HAS_ARA 0
#endif

class GainARAProcessor : public lattice::Processor {
public:
    GainARAProcessor();
    ~GainARAProcessor() override;

    void process(float** inputs, float** outputs, std::size_t blockSize) override;
    void setParameter(int paramId, double value) override;
    void onMessageFromWebView(const nlohmann::json& j) override;
    void prepareToPlay(double sampleRate, uint32_t minFrameCount, uint32_t maxFrameCount) override;
    void onWebViewIsReady() override;
    void emitAraEventFromDocumentController(const ARA::PlugIn::DocumentController* documentController, const nlohmann::json& event);

#if LATTICE_GAINARA_HAS_ARA
    static const ARA::ARAFactory* getStaticAraFactory() noexcept;
#endif

private:
    void enqueueAraUiEvent(const nlohmann::json& event);
    void flushAraUiEvents();

    std::mutex araUiQueueMutex;
    std::deque<nlohmann::json> araUiQueue;
    std::atomic<bool> webViewReady{false};

#if LATTICE_GAINARA_HAS_ARA
    ARA::PlugIn::PlugInExtension araExtension;
#endif
};
