#include "GainARAProcessor.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <deque>
#include <iterator>
#include <limits>
#include <mutex>
#include <vector>

//===================================================================================
pluginType* LatticeProcessorPluginFactory::createPlugin(const clap_host* host)
{
    auto* processor = new GainARAProcessor();
    return new pluginType(host, *processor);
}
//===================================================================================

#if LATTICE_GAINARA_HAS_ARA
namespace {
thread_local GainARAProcessor* gCurrentBindingProcessor = nullptr;
std::mutex gPreInstanceAraQueueMutex;
std::deque<nlohmann::json> gPreInstanceAraQueue;
std::mutex gSharedAraUiQueueMutex;
std::deque<nlohmann::json> gSharedAraUiQueue;

void setCurrentBindingProcessor(GainARAProcessor* processor)
{
    gCurrentBindingProcessor = processor;
}

void enqueuePreInstanceAraEvent(const nlohmann::json& event)
{
    std::lock_guard<std::mutex> lock(gPreInstanceAraQueueMutex);
    gPreInstanceAraQueue.push_back(event);
}

void consumePreInstanceAraEvents(std::deque<nlohmann::json>& destination)
{
    std::lock_guard<std::mutex> lock(gPreInstanceAraQueueMutex);
    if (gPreInstanceAraQueue.empty())
        return;

    destination.insert(destination.end(),
                       std::make_move_iterator(gPreInstanceAraQueue.begin()),
                       std::make_move_iterator(gPreInstanceAraQueue.end()));
    gPreInstanceAraQueue.clear();
}

struct AudioDiagnostics
{
    double avgRms = 0.0;
    double peakAbs = 0.0;
    double crest = 0.0;
    double peakWindowRms = 0.0;
    double sampleRate = 0.0;
    double durationSeconds = 0.0;
    ARA::ARAChannelCount channelCount = 0;
    ARA::ARASampleCount sampleCount = 0;
};

AudioDiagnostics analyzeAudioSource(ARA::PlugIn::AudioSource* audioSource)
{
    AudioDiagnostics result;

    if ((audioSource == nullptr) || !audioSource->isSampleAccessEnabled())
        return result;

    const auto channelCount = audioSource->getChannelCount();
    const auto totalSamples = audioSource->getSampleCount();
    const auto sampleRate = audioSource->getSampleRate();

    result.channelCount = channelCount;
    result.sampleCount = totalSamples;
    result.sampleRate = sampleRate;

    if (sampleRate > 0.0)
        result.durationSeconds = static_cast<double>(totalSamples) / sampleRate;

    if ((channelCount == 0) || (totalSamples == 0))
        return result;

    constexpr ARA::ARASampleCount blockSize = 8192;

    ARA::PlugIn::HostAudioReader audioReader { audioSource };
    std::vector<std::vector<float>> channelBuffers(static_cast<size_t>(channelCount), std::vector<float>(static_cast<size_t>(blockSize), 0.0f));
    std::vector<void*> dataPointers(static_cast<size_t>(channelCount), nullptr);

    double sumSquares = 0.0;
    double peakAbs = 0.0;
    double peakWindowRms = 0.0;

    for (ARA::ARASampleCount start = 0; start < totalSamples; start += blockSize)
    {
        const auto framesToRead = std::min(blockSize, totalSamples - start);

        for (ARA::ARAChannelCount channel = 0; channel < channelCount; ++channel)
            dataPointers[static_cast<size_t>(channel)] = channelBuffers[static_cast<size_t>(channel)].data();

        audioReader.readAudioSamples(start, framesToRead, dataPointers.data());

        double windowSumSquares = 0.0;
        for (ARA::ARAChannelCount channel = 0; channel < channelCount; ++channel)
        {
            auto& buffer = channelBuffers[static_cast<size_t>(channel)];
            for (ARA::ARASampleCount i = 0; i < framesToRead; ++i)
            {
                const auto sample = static_cast<double>(buffer[static_cast<size_t>(i)]);
                const auto absSample = std::abs(sample);
                peakAbs = std::max(peakAbs, absSample);
                const auto squared = sample * sample;
                sumSquares += squared;
                windowSumSquares += squared;
            }
        }

        const auto windowFrameCount = static_cast<double>(framesToRead) * static_cast<double>(channelCount);
        if (windowFrameCount > 0.0)
        {
            const auto windowRms = std::sqrt(windowSumSquares / windowFrameCount);
            peakWindowRms = std::max(peakWindowRms, windowRms);
        }
    }

    const auto frameCount = static_cast<double>(totalSamples) * static_cast<double>(channelCount);
    if (frameCount > 0.0)
        result.avgRms = std::sqrt(sumSquares / frameCount);

    result.peakAbs = peakAbs;
    result.peakWindowRms = peakWindowRms;
    result.crest = (result.avgRms > std::numeric_limits<double>::epsilon()) ? (peakAbs / result.avgRms) : 0.0;

    return result;
}


nlohmann::json createDiagnosticsEvent(const char* callbackName, const AudioDiagnostics& diagnostics)
{
    return {
        {"command", "araDiagnostics"},
        {"data", {
            {"callback", callbackName},
            {"channels", diagnostics.channelCount},
            {"samples", diagnostics.sampleCount},
            {"sampleRate", diagnostics.sampleRate},
            {"durationSeconds", diagnostics.durationSeconds},
            {"avgRms", diagnostics.avgRms},
            {"peakAbs", diagnostics.peakAbs},
            {"crest", diagnostics.crest},
            {"peakWindowRms", diagnostics.peakWindowRms}
        }}
    };
}

class GainARADocumentController : public ARA::PlugIn::DocumentController {
public:
    GainARADocumentController(const ARA::PlugIn::PlugInEntry* entry, const ARA::ARADocumentControllerHostInstance* instance) noexcept
        : ARA::PlugIn::DocumentController(entry, instance), ownerProcessor(gCurrentBindingProcessor)
    {
    }

protected:
    bool doRestoreObjectsFromArchive(ARA::PlugIn::HostArchiveReader* /*archiveReader*/, const ARA::PlugIn::RestoreObjectsFilter* /*filter*/) noexcept override
    {
        lattice::logDebug << "[GainARA][ARA] doRestoreObjectsFromArchive";
        
        if (ownerProcessor)
            ownerProcessor->emitAraEventFromDocumentController(this, {{"command", "araLifecycle"}, {"data", {{"callback", "doRestoreObjectsFromArchive"}}}});
        else
            enqueuePreInstanceAraEvent({{"command", "araLifecycle"}, {"data", {{"callback", "doRestoreObjectsFromArchive"}}}});
        return true;
    }

    bool doStoreObjectsToArchive(ARA::PlugIn::HostArchiveWriter* /*archiveWriter*/, const ARA::PlugIn::StoreObjectsFilter* /*filter*/) noexcept override
    {
        lattice::logDebug << "[GainARA][ARA] doStoreObjectsToArchive";
        
        if (ownerProcessor)
            ownerProcessor->emitAraEventFromDocumentController(this, {{"command", "araLifecycle"}, {"data", {{"callback", "doStoreObjectsToArchive"}}}});
        else
            enqueuePreInstanceAraEvent({{"command", "araLifecycle"}, {"data", {{"callback", "doStoreObjectsToArchive"}}}});
        return true;
    }

    void doUpdateMusicalContextContent(ARA::PlugIn::MusicalContext* /*musicalContext*/, const ARA::ARAContentTimeRange* /*range*/, ARA::ContentUpdateScopes /*scopeFlags*/) noexcept override
    {
        lattice::logDebug << "[GainARA][ARA] doUpdateMusicalContextContent";
        
        if (ownerProcessor)
            ownerProcessor->emitAraEventFromDocumentController(this, {{"command", "araLifecycle"}, {"data", {{"callback", "doUpdateMusicalContextContent"}}}});
        else
            enqueuePreInstanceAraEvent({{"command", "araLifecycle"}, {"data", {{"callback", "doUpdateMusicalContextContent"}}}});
    }

    void doUpdateAudioSourceContent(ARA::PlugIn::AudioSource* audioSource, const ARA::ARAContentTimeRange* /*range*/, ARA::ContentUpdateScopes scopeFlags) noexcept override
    {
        lattice::logDebug << "[GainARA][ARA] doUpdateAudioSourceContent";
        if (ownerProcessor)
            ownerProcessor->emitAraEventFromDocumentController(this, {{"command", "araLifecycle"}, {"data", {{"callback", "doUpdateAudioSourceContent"}}}});
        else
            enqueuePreInstanceAraEvent({{"command", "araLifecycle"}, {"data", {{"callback", "doUpdateAudioSourceContent"}}}});
        
            if ((audioSource != nullptr) && audioSource->isSampleAccessEnabled() && scopeFlags.affectSamples())
        {
            const auto diagnostics = analyzeAudioSource(audioSource);

            if (ownerProcessor)
                ownerProcessor->emitAraEventFromDocumentController(this, createDiagnosticsEvent("doUpdateAudioSourceContent", diagnostics));
            else
                enqueuePreInstanceAraEvent(createDiagnosticsEvent("doUpdateAudioSourceContent", diagnostics));
        }
    }

    void willEnableAudioSourceSamplesAccess(ARA::PlugIn::AudioSource* /*audioSource*/, bool enable) noexcept override
    {
        lattice::logDebug << "[GainARA][ARA] willEnableAudioSourceSamplesAccess enable=" << (enable ? "true" : "false");
        
        if (ownerProcessor)
            ownerProcessor->emitAraEventFromDocumentController(this, {{"command", "araLifecycle"}, {"data", {{"callback", "willEnableAudioSourceSamplesAccess"}, {"enable", enable}}}});
        else
            enqueuePreInstanceAraEvent({{"command", "araLifecycle"}, {"data", {{"callback", "willEnableAudioSourceSamplesAccess"}, {"enable", enable}}}});
    }

    void didEnableAudioSourceSamplesAccess(ARA::PlugIn::AudioSource* audioSource, bool enable) noexcept override
    {
        lattice::logDebug << "[GainARA][ARA] didEnableAudioSourceSamplesAccess enable=" << (enable ? "true" : "false");
        
        if (ownerProcessor)
            ownerProcessor->emitAraEventFromDocumentController(this, {{"command", "araLifecycle"}, {"data", {{"callback", "didEnableAudioSourceSamplesAccess"}, {"enable", enable}}}});
        else
            enqueuePreInstanceAraEvent({{"command", "araLifecycle"}, {"data", {{"callback", "didEnableAudioSourceSamplesAccess"}, {"enable", enable}}}});
        
            if (enable)
        {
            const auto diagnostics = analyzeAudioSource(audioSource);

            if (ownerProcessor)
                ownerProcessor->emitAraEventFromDocumentController(this, createDiagnosticsEvent("didEnableAudioSourceSamplesAccess", diagnostics));
            else
                enqueuePreInstanceAraEvent(createDiagnosticsEvent("didEnableAudioSourceSamplesAccess", diagnostics));
        }
    }

private:
    GainARAProcessor* ownerProcessor = nullptr;
};

class GainARAFactoryConfig : public ARA::PlugIn::FactoryConfig {
public:
    const char* getFactoryID() const noexcept override { return "com.cabbageaudio.gainara.factory"; }
    const char* getPlugInName() const noexcept override { return "GainARAPlugin"; }
    const char* getManufacturerName() const noexcept override { return "CabbageAudio"; }
    const char* getInformationURL() const noexcept override { return "https://cabbageaudio.com"; }
    const char* getVersion() const noexcept override { return "1.0.0"; }
    const char* getDocumentArchiveID() const noexcept override { return "com.cabbageaudio.gainara.document.v1"; }
};

const ARA::ARAFactory* getGainAraFactory() noexcept
{
    return ARA::PlugIn::PlugInEntry::getPlugInEntry<GainARAFactoryConfig, GainARADocumentController>()->getFactory();
}
} // namespace

const ARA::ARAFactory* latticeGetAraFactory()
{
    return GainARAProcessor::getStaticAraFactory();
}

const ARA::ARAFactory* GainARAProcessor::getStaticAraFactory() noexcept
{
    return getGainAraFactory();
}
#endif

GainARAProcessor::GainARAProcessor()
    : Processor()
{
    addInputBus("Input Bus", 2, lattice::ChannelLayout::Stereo);
    addOutputBus("Output Bus", 2, lattice::ChannelLayout::Stereo);

    addParameter({ "Gain", 0, 1 });

    setEditorSize(800, 600);

    araCallback = [this](const ARA::PlugIn::DocumentController* /*documentController*/, const nlohmann::json& event) {
        enqueueAraUiEvent(event);
    };

#if LATTICE_GAINARA_HAS_ARA
    getAraFactory = []() -> const void* {
        return reinterpret_cast<const void*>(GainARAProcessor::getStaticAraFactory());
    };

    bindToAraDocumentController = [this](void* documentControllerRef, uint32_t knownRoles, uint32_t assignedRoles) -> const void* {
        setCurrentBindingProcessor(this);
        const auto* instance = araExtension.bindToARA(
            reinterpret_cast<ARA::ARADocumentControllerRef>(documentControllerRef),
            static_cast<ARA::ARAPlugInInstanceRoleFlags>(knownRoles),
            static_cast<ARA::ARAPlugInInstanceRoleFlags>(assignedRoles));
        setCurrentBindingProcessor(nullptr);

        return reinterpret_cast<const void*>(instance);
    };
#endif
}

GainARAProcessor::~GainARAProcessor()
{}

void GainARAProcessor::process(float** inputs, float** outputs, std::size_t blockSize)
{
    if (webViewReady.load(std::memory_order_acquire))
        flushAraUiEvents();

    const auto channels = getChannelConfig().getTotalNumInputChannels();

    for (uint32_t i = 0; i < blockSize; i++)
    {
        for (uint32_t ch = 0; ch < channels; ++ch)
        {
            outputs[ch][i] = inputs[ch][i] * getParameterValue("Gain");
        }
    }
}

void GainARAProcessor::onMessageFromWebView(const nlohmann::json& j)
{
    const auto& payload = (j.is_array() && !j.empty()) ? j.at(0) : j;
    lattice::logDebug << payload.dump(4);

    if (!payload.is_object())
        return;

    float value = payload.value("value", 0.f);
    auto paramIdx = payload.value("paramIdx", -1);
    auto gesture = payload.value("gesture", "gesture");

    if (paramIdx < 0 || static_cast<size_t>(paramIdx) >= getParameters().size())
        return;

    if (gesture == "begin") {
        addParameterChange({paramIdx, getParameter(paramIdx).toNormalised(value), lattice::ParamChangeType::GestureBegin});
    } else if (gesture == "value") {
        addParameterChange({paramIdx, getParameter(paramIdx).toNormalised(value), lattice::ParamChangeType::Value});
    } else if (gesture == "end") {
        addParameterChange({paramIdx, getParameter(paramIdx).toNormalised(value), lattice::ParamChangeType::GestureEnd});
    }

    getParameters()[paramIdx].value = value;
}

void GainARAProcessor::setParameter(int paramId, double value)
{
    getParameters()[paramId].value = getParameter(paramId).fromNormalised(value);
}

void GainARAProcessor::prepareToPlay(double /*sampleRate*/, uint32_t /*minFrameCount*/, uint32_t /*maxFrameCount*/)
{
}

void GainARAProcessor::emitAraEventFromDocumentController(const ARA::PlugIn::DocumentController* documentController, const nlohmann::json& event)
{
    araCallback(documentController, event);
}

void GainARAProcessor::onWebViewIsReady()
{
    webViewReady.store(true, std::memory_order_release);
    flushAraUiEvents();

    std::deque<nlohmann::json> preInstanceEvents;
    consumePreInstanceAraEvents(preInstanceEvents);
    for (const auto& event : preInstanceEvents)
    {
        sendWebViewMessage(event);
        lattice::logDebug << event.dump(4);
    }
        
}

void GainARAProcessor::enqueueAraUiEvent(const nlohmann::json& event)
{
    std::lock_guard<std::mutex> lock(gSharedAraUiQueueMutex);
    gSharedAraUiQueue.push_back(event);
}

void GainARAProcessor::flushAraUiEvents()
{
    std::deque<nlohmann::json> pending;
    {
        std::lock_guard<std::mutex> lock(gSharedAraUiQueueMutex);
        if (gSharedAraUiQueue.empty())
            return;
        pending.swap(gSharedAraUiQueue);
    }

    for (const auto& event : pending)
        sendWebViewMessage(event);
}
