#pragma once

#include "lattice/LatticeAraProcessor.h"

#if LATTICE_HAS_ARA

class GainARAProcessor : public lattice::AraProcessor<GainARAProcessor>
{
public:
    GainARAProcessor();
    ~GainARAProcessor() override;

    // Required: supply ARA factory identity.
    static lattice::AraPluginInfo getStaticAraInfo() noexcept;

    // Standard Processor overrides.
    void process(float** inputs, float** outputs, std::size_t blockSize) override;
    void setParameter(int paramId, double value) override;
    void onMessageFromWebView(const nlohmann::json& j) override;
    void prepareToPlay(double sampleRate, uint32_t minFrameCount, uint32_t maxFrameCount) override;

    // ARA callbacks — only override what this plugin needs.
    void araAudioSourceContentUpdated(ARA::PlugIn::AudioSource* source,
                                       ARA::ContentUpdateScopes scopes) override;
    void araDidEnableSamplesAccess(ARA::PlugIn::AudioSource* source, bool enable) override;
    void araBeginEditing() override;
    void araEndEditing() override;
    void araDidNotifyModelUpdates() override;
    void araDocumentPropertiesUpdated(ARA::PlugIn::Document* document) override;
};

#endif // LATTICE_HAS_ARA
