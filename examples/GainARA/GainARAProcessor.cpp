#include "GainARAProcessor.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

//===================================================================================
pluginType* LatticeProcessorPluginFactory::createPlugin(const clap_host* host)
{
    auto* processor = new GainARAProcessor();
    return new pluginType(host, *processor);
}

// Defines the latticeGetAraFactory() free function used by the CLAP factory.
LATTICE_DEFINE_ARA_FACTORY(GainARAProcessor)
//===================================================================================

namespace {

struct AudioDiagnostics
{
    double avgRms = 0.0;
    double peakAbs = 0.0;
    double crest = 0.0;
    double peakWindowRms = 0.0;
    double sampleRate = 0.0;
    double durationSeconds = 0.0;
    ARA::ARAChannelCount channelCount = 0;
    ARA::ARASampleCount  sampleCount  = 0;
};

AudioDiagnostics analyzeAudioSource(ARA::PlugIn::AudioSource* audioSource)
{
    AudioDiagnostics result;

    if ((audioSource == nullptr) || !audioSource->isSampleAccessEnabled())
        return result;

    const auto channelCount = audioSource->getChannelCount();
    const auto totalSamples = audioSource->getSampleCount();
    const auto sampleRate   = audioSource->getSampleRate();

    result.channelCount = channelCount;
    result.sampleCount  = totalSamples;
    result.sampleRate   = sampleRate;

    if (sampleRate > 0.0)
        result.durationSeconds = static_cast<double>(totalSamples) / sampleRate;

    if ((channelCount == 0) || (totalSamples == 0))
        return result;

    constexpr ARA::ARASampleCount blockSize = 8192;

    ARA::PlugIn::HostAudioReader audioReader { audioSource };
    std::vector<std::vector<float>> channelBuffers(static_cast<size_t>(channelCount),
                                                   std::vector<float>(static_cast<size_t>(blockSize), 0.0f));
    std::vector<void*> dataPointers(static_cast<size_t>(channelCount), nullptr);

    double sumSquares    = 0.0;
    double peakAbs       = 0.0;
    double peakWindowRms = 0.0;

    for (ARA::ARASampleCount start = 0; start < totalSamples; start += blockSize)
    {
        const auto framesToRead = std::min(blockSize, totalSamples - start);

        for (ARA::ARAChannelCount ch = 0; ch < channelCount; ++ch)
            dataPointers[static_cast<size_t>(ch)] = channelBuffers[static_cast<size_t>(ch)].data();

        audioReader.readAudioSamples(start, framesToRead, dataPointers.data());

        double windowSumSquares = 0.0;
        for (ARA::ARAChannelCount ch = 0; ch < channelCount; ++ch)
        {
            auto& buffer = channelBuffers[static_cast<size_t>(ch)];
            for (ARA::ARASampleCount i = 0; i < framesToRead; ++i)
            {
                const auto sample    = static_cast<double>(buffer[static_cast<size_t>(i)]);
                const auto absSample = std::abs(sample);
                peakAbs              = std::max(peakAbs, absSample);
                const auto squared   = sample * sample;
                sumSquares          += squared;
                windowSumSquares    += squared;
            }
        }

        const auto windowFrameCount = static_cast<double>(framesToRead) * static_cast<double>(channelCount);
        if (windowFrameCount > 0.0)
            peakWindowRms = std::max(peakWindowRms, std::sqrt(windowSumSquares / windowFrameCount));
    }

    const auto frameCount = static_cast<double>(totalSamples) * static_cast<double>(channelCount);
    if (frameCount > 0.0)
        result.avgRms = std::sqrt(sumSquares / frameCount);

    result.peakAbs       = peakAbs;
    result.peakWindowRms = peakWindowRms;
    result.crest         = (result.avgRms > std::numeric_limits<double>::epsilon())
                               ? (peakAbs / result.avgRms)
                               : 0.0;
    return result;
}

nlohmann::json createDiagnosticsEvent(const char* callbackName, const AudioDiagnostics& d)
{
    return {
        {"command", "araDiagnostics"},
        {"data", {
            {"callback",        callbackName},
            {"channels",        d.channelCount},
            {"samples",         d.sampleCount},
            {"sampleRate",      d.sampleRate},
            {"durationSeconds", d.durationSeconds},
            {"avgRms",          d.avgRms},
            {"peakAbs",         d.peakAbs},
            {"crest",           d.crest},
            {"peakWindowRms",   d.peakWindowRms}
        }}
    };
}

} // namespace

//===================================================================================
lattice::AraPluginInfo GainARAProcessor::getStaticAraInfo() noexcept
{
    return {
        "com.cabbageaudio.gainara.factory",
        "GainARAPlugin",
        "CabbageAudio",
        "https://cabbageaudio.com",
        "1.0.0",
        "com.cabbageaudio.gainara.document.v1"
    };
}

GainARAProcessor::GainARAProcessor()
    : AraProcessor()
{
    addAudioPortsConfig("Stereo", "2", "2");
    addParameter({ "Gain", 0, 1 });
    setEditorSize(800, 600);
}

GainARAProcessor::~GainARAProcessor()
{}

void GainARAProcessor::process(float** inputs, float** outputs, std::size_t blockSize)
{
    flushAraEvents();

    const auto channels = getChannelConfig().getTotalNumInputChannels();
    for (uint32_t i = 0; i < blockSize; i++)
        for (uint32_t ch = 0; ch < channels; ++ch)
            outputs[ch][i] = inputs[ch][i] * getParameterValue("Gain");
}

void GainARAProcessor::araAudioSourceContentUpdated(ARA::PlugIn::AudioSource* source,
                                                     ARA::ContentUpdateScopes scopes)
{
    lattice::logDebug << "araAudioSourceContentUpdated: source=" << (void*)source
                      << " sampleAccessEnabled=" << source->isSampleAccessEnabled()
                      << " affectsSamples=" << scopes.affectSamples();

    if (source->isSampleAccessEnabled() && scopes.affectSamples())
        sendAraEvent(createDiagnosticsEvent("doUpdateAudioSourceContent", analyzeAudioSource(source)));
}

void GainARAProcessor::araDidEnableSamplesAccess(ARA::PlugIn::AudioSource* source, bool enable)
{
    lattice::logDebug << "araDidEnableSamplesAccess: source=" << (void*)source
                      << " enable=" << enable
                      << " sampleAccessEnabled=" << source->isSampleAccessEnabled();

    if (enable)
        sendAraEvent(createDiagnosticsEvent("didEnableAudioSourceSamplesAccess", analyzeAudioSource(source)));
}

void GainARAProcessor::onMessageFromWebView(const nlohmann::json& j)
{
    const auto& payload = (j.is_array() && !j.empty()) ? j.at(0) : j;
    lattice::logDebug << payload.dump(4);

    if (!payload.is_object())
        return;

    float value    = payload.value("value", 0.f);
    auto  paramIdx = payload.value("paramIdx", -1);
    auto  gesture  = payload.value("gesture", "gesture");

    if (paramIdx < 0 || static_cast<size_t>(paramIdx) >= getParameters().size())
        return;

    if (gesture == "begin")
        addParameterChange({paramIdx, getParameter(paramIdx).toNormalised(value), lattice::ParamChangeType::GestureBegin});
    else if (gesture == "value")
        addParameterChange({paramIdx, getParameter(paramIdx).toNormalised(value), lattice::ParamChangeType::Value});
    else if (gesture == "end")
        addParameterChange({paramIdx, getParameter(paramIdx).toNormalised(value), lattice::ParamChangeType::GestureEnd});

    getParameters()[paramIdx].value = value;
}

void GainARAProcessor::setParameter(int paramId, double value)
{
    getParameters()[paramId].value = getParameter(paramId).fromNormalised(value);
}

void GainARAProcessor::prepareToPlay(double /*sampleRate*/, uint32_t /*minFrameCount*/, uint32_t /*maxFrameCount*/)
{
}
