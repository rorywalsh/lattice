/*
    MIT License

    Copyright (c) 2025 Rory Walsh

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

#pragma once

#if __has_include(<ARA_API/ARACLAP.h>) && __has_include(<ARA_Library/PlugIn/ARAPlug.h>)
#include <ARA_API/ARACLAP.h>
#include <ARA_Library/PlugIn/ARAPlug.h>
#define LATTICE_HAS_ARA 1
#else
#define LATTICE_HAS_ARA 0
#endif

#include "LatticeProcessor.h"

#include <atomic>
#include <deque>
#include <mutex>
#include <vector>

#if LATTICE_HAS_ARA

namespace lattice {

// ============================================================================
// AraPluginInfo — static identity provided by the user's processor class.
// All strings must have static storage duration (string literals are fine).
// ============================================================================
struct AraPluginInfo
{
    const char* factoryId;
    const char* pluginName;
    const char* manufacturer;
    const char* informationUrl;
    const char* version;
    const char* documentArchiveId;
};

// Forward declaration for detail helpers.
template<typename Derived> class AraProcessor;

namespace detail {

// ---------------------------------------------------------------------------
// Per-Derived-type thread-local binding pointer.
// Used only during the first bindToARA() call to give the newly constructed
// DocumentController a reference to the first processor instance.
// ---------------------------------------------------------------------------
template<typename Derived>
inline thread_local AraProcessor<Derived>* currentBinding = nullptr;

// ---------------------------------------------------------------------------
// LatticeDocumentController<Derived>
//
// Handles every ARA DocumentController callback. Each callback:
//   1. Calls the corresponding virtual on the bound AraProcessor (if bound).
//   2. Broadcasts a lightweight lifecycle JSON event to every registered
//      processor instance so each can forward it to its own webview.
//
// Multiple instances can be bound to the same document controller (e.g. a
// session opened with many instances and no UIs). Each calls addProcessor()
// after binding, and receives the full pending backlog at that point so no
// startup events are missed regardless of which UI opens first.
// ---------------------------------------------------------------------------
template<typename Derived>
class LatticeDocumentController : public ARA::PlugIn::DocumentController
{
public:
    LatticeDocumentController(const ARA::PlugIn::PlugInEntry* entry,
                               const ARA::ARADocumentControllerHostInstance* instance) noexcept
        : ARA::PlugIn::DocumentController(entry, instance)
    {
        // The first processor to trigger DC construction is available via
        // the thread-local; register it immediately.
        if (auto* p = currentBinding<Derived>)
            addProcessor(p);
    }

    // Called by each processor instance after it successfully binds to this DC.
    // Delivers the full event backlog accumulated before this instance registered,
    // so that any UI opened later gets complete startup state.
    void addProcessor(AraProcessor<Derived>* p)
    {
        std::lock_guard<std::mutex> lock(controllerMutex);
        processors.push_back(p);
        // Deliver the accumulated backlog to this new subscriber.
        for (const auto& ev : eventBacklog)
            p->dispatchAraEvent(ev);
        // Late-registration replay: if this processor joins after the host already
        // enabled sample access on some sources (common when the UI instance is
        // created after the rendering instance), araDidEnableSamplesAccess was never
        // called on it.  Replay enable=true for every source that is currently
        // accessible so araAccessibleSources is populated correctly.
        if (const auto* doc = this->getDocument())
        {
            for (auto* source : doc->getAudioSources())
            {
                if (source->isSampleAccessEnabled())
                    p->araDidEnableSamplesAccess(source, true);
            }
        }
    }

protected:
    void emitOrQueue(const nlohmann::json& event) const
    {
        std::lock_guard<std::mutex> lock(controllerMutex);
        if (processors.empty())
        {
            // No processors bound yet — buffer the event for late subscribers.
            eventBacklog.push_back(event);
        }
        else
        {
            // Broadcast to every bound instance; each routes to its own queue.
            for (auto* p : processors)
                p->dispatchAraEvent(event);
        }
    }

    bool doRestoreObjectsFromArchive(ARA::PlugIn::HostArchiveReader* /*reader*/,
                                     const ARA::PlugIn::RestoreObjectsFilter* /*filter*/) noexcept override
    {
        // Archive operations are document-level; delegate to the first instance.
        bool ok = true;
        {
            std::lock_guard<std::mutex> lock(controllerMutex);
            if (!processors.empty()) ok = processors.front()->araRestoreFromArchive();
        }
        emitOrQueue({{"command", "araLifecycle"}, {"data", {{"callback", "doRestoreObjectsFromArchive"}}}});
        return ok;
    }

    bool doStoreObjectsToArchive(ARA::PlugIn::HostArchiveWriter* /*writer*/,
                                  const ARA::PlugIn::StoreObjectsFilter* /*filter*/) noexcept override
    {
        bool ok = true;
        {
            std::lock_guard<std::mutex> lock(controllerMutex);
            if (!processors.empty()) ok = processors.front()->araStoreToArchive();
        }
        emitOrQueue({{"command", "araLifecycle"}, {"data", {{"callback", "doStoreObjectsToArchive"}}}});
        return ok;
    }

    void doUpdateMusicalContextContent(ARA::PlugIn::MusicalContext* ctx,
                                        const ARA::ARAContentTimeRange* /*range*/,
                                        ARA::ContentUpdateScopes scopes) noexcept override
    {
        {
            std::lock_guard<std::mutex> lock(controllerMutex);
            for (auto* p : processors) p->araMusicalContextUpdated(ctx, scopes);
        }
        emitOrQueue({{"command", "araLifecycle"}, {"data", {{"callback", "doUpdateMusicalContextContent"}}}});
    }

    void doUpdateAudioSourceContent(ARA::PlugIn::AudioSource* source,
                                     const ARA::ARAContentTimeRange* /*range*/,
                                     ARA::ContentUpdateScopes scopes) noexcept override
    {
        {
            std::lock_guard<std::mutex> lock(controllerMutex);
            for (auto* p : processors) p->araAudioSourceContentUpdated(source, scopes);
        }
        emitOrQueue({{"command", "araLifecycle"}, {"data", {{"callback", "doUpdateAudioSourceContent"}}}});
    }

    void willEnableAudioSourceSamplesAccess(ARA::PlugIn::AudioSource* source, bool enable) noexcept override
    {
        {
            std::lock_guard<std::mutex> lock(controllerMutex);
            for (auto* p : processors) p->araWillEnableSamplesAccess(source, enable);
        }
        emitOrQueue({{"command", "araLifecycle"}, {"data", {{"callback", "willEnableAudioSourceSamplesAccess"}, {"enable", enable}}}});
    }

    void didEnableAudioSourceSamplesAccess(ARA::PlugIn::AudioSource* source, bool enable) noexcept override
    {
        {
            std::lock_guard<std::mutex> lock(controllerMutex);
            for (auto* p : processors) p->araDidEnableSamplesAccess(source, enable);
        }
        emitOrQueue({{"command", "araLifecycle"}, {"data", {{"callback", "didEnableAudioSourceSamplesAccess"}, {"enable", enable}}}});
    }

    void didUpdatePlaybackRegionProperties(ARA::PlugIn::PlaybackRegion* playbackRegion) noexcept override
    {
        {
            std::lock_guard<std::mutex> lock(controllerMutex);
            for (auto* p : processors)
                p->araPlaybackRegionPropertiesUpdated(playbackRegion);
        }
        emitOrQueue({{"command", "araLifecycle"},
                     {"data", {{"callback", "didUpdatePlaybackRegionProperties"}}}});
    }

    void willBeginEditing() noexcept override
    {
        {
            std::lock_guard<std::mutex> lock(controllerMutex);
            for (auto* p : processors) p->araBeginEditing();
        }
        emitOrQueue({{"command", "araLifecycle"}, {"data", {{"callback", "willBeginEditing"}}}});
    }

    void didEndEditing() noexcept override
    {
        {
            std::lock_guard<std::mutex> lock(controllerMutex);
            for (auto* p : processors) p->araEndEditing();
        }
        emitOrQueue({{"command", "araLifecycle"}, {"data", {{"callback", "didEndEditing"}}}});
    }

    void willNotifyModelUpdates() noexcept override
    {
        emitOrQueue({{"command", "araLifecycle"}, {"data", {{"callback", "willNotifyModelUpdates"}}}});
    }

    void didNotifyModelUpdates() noexcept override
    {
        {
            std::lock_guard<std::mutex> lock(controllerMutex);
            for (auto* p : processors) p->araDidNotifyModelUpdates();
        }
        emitOrQueue({{"command", "araLifecycle"}, {"data", {{"callback", "didNotifyModelUpdates"}}}});
    }

    void didUpdateDocumentProperties(ARA::PlugIn::Document* document) noexcept override
    {
        {
            std::lock_guard<std::mutex> lock(controllerMutex);
            for (auto* p : processors) p->araDocumentPropertiesUpdated(document);
        }
        emitOrQueue({{"command", "araLifecycle"},
                     {"data", {{"callback", "didUpdateDocumentProperties"}}}});
    }

private:
    mutable std::mutex                  controllerMutex;
    std::vector<AraProcessor<Derived>*> processors;
    mutable std::deque<nlohmann::json>  eventBacklog; // events before first processor registered
};

// ---------------------------------------------------------------------------
// LatticeFactoryConfig<Derived>
// Reads identity strings from Derived::getStaticAraInfo() at runtime.
// One instantiation per Derived type satisfies ARA's factory singleton
// requirement.
// ---------------------------------------------------------------------------
template<typename Derived>
class LatticeFactoryConfig : public ARA::PlugIn::FactoryConfig
{
    const char* getFactoryID()        const noexcept override { return Derived::getStaticAraInfo().factoryId;        }
    const char* getPlugInName()       const noexcept override { return Derived::getStaticAraInfo().pluginName;       }
    const char* getManufacturerName() const noexcept override { return Derived::getStaticAraInfo().manufacturer;     }
    const char* getInformationURL()   const noexcept override { return Derived::getStaticAraInfo().informationUrl;   }
    const char* getVersion()          const noexcept override { return Derived::getStaticAraInfo().version;          }
    const char* getDocumentArchiveID()const noexcept override { return Derived::getStaticAraInfo().documentArchiveId;}
};

} // namespace detail

// ============================================================================
// AraProcessor<Derived> — CRTP base class for ARA-enabled plugins.
//
// Usage:
//   1. Inherit: class MyPlugin : public lattice::AraProcessor<MyPlugin>
//   2. Define:  static lattice::AraPluginInfo getStaticAraInfo() noexcept { ... }
//   3. Override any ARA virtual callbacks you need (all default to no-ops).
//   4. Call flushAraEvents() in your process() implementation.
//   5. Put LATTICE_DEFINE_ARA_FACTORY(MyPlugin) in your .cpp file.
//
// Everything else — the DocumentController, FactoryConfig, thread-local
// binding mechanism, pre-instance queue, and UI event queue — is hidden here.
// ============================================================================
template<typename Derived>
class AraProcessor : public Processor
{
public:
    // -------------------------------------------------------------------------
    // ARA virtual callbacks — override whichever your plugin needs.
    // These are called on whichever thread the host uses for ARA notifications;
    // implementations must be thread-safe and exception-safe (noexcept context).
    // -------------------------------------------------------------------------

    // Called when the host wants to restore state from an archive.
    // Return false to indicate failure.
    virtual bool araRestoreFromArchive() { return true; }

    // Called when the host wants to store state to an archive.
    // Return false to indicate failure.
    virtual bool araStoreToArchive() { return true; }

    // Called when the musical context (tempo, time signature, etc.) changes.
    virtual void araMusicalContextUpdated(ARA::PlugIn::MusicalContext* /*ctx*/,
                                          ARA::ContentUpdateScopes /*scopes*/) {}

    // Called when audio source content (samples, chord track, etc.) changes.
    virtual void araAudioSourceContentUpdated(ARA::PlugIn::AudioSource* /*source*/,
                                               ARA::ContentUpdateScopes /*scopes*/) {}

    // Called just before the host enables or disables sample access on a source.
    virtual void araWillEnableSamplesAccess(ARA::PlugIn::AudioSource* /*source*/, bool /*enable*/) {}

    // Called after the host enables or disables sample access on a source.
    // When enable == true, the source is ready to be read via HostAudioReader.
    virtual void araDidEnableSamplesAccess(ARA::PlugIn::AudioSource* /*source*/, bool /*enable*/) {}

    // Called when the host updates playback region properties (e.g. region resized/moved).
    virtual void araPlaybackRegionPropertiesUpdated(ARA::PlugIn::PlaybackRegion* /*playbackRegion*/) {}

    // Called when the host begins an edit cycle.
    virtual void araBeginEditing() {}

    // Called when the host ends an edit cycle.
    virtual void araEndEditing() {}

    // Called after the host has processed model updates.
    virtual void araDidNotifyModelUpdates() {}

    // Called when document properties are updated.
    virtual void araDocumentPropertiesUpdated(ARA::PlugIn::Document* /*document*/) {}

    // -------------------------------------------------------------------------
    // Webview event helpers
    // -------------------------------------------------------------------------

    // Post a JSON event to the webview (thread-safe).
    // Events are queued until the webview is ready, then forwarded on process().
    void sendAraEvent(const nlohmann::json& event)
    {
        std::lock_guard<std::mutex> lock(araQueueMutex);
        araQueue.push_back(event);
    }

    // Forward all queued ARA events to the webview.
    // Call this once per block from your process() implementation.
    // No-ops until onWebViewIsReady() has fired.
    void flushAraEvents()
    {
        if (!webViewReady.load(std::memory_order_acquire))
            return;

        std::deque<nlohmann::json> pending;
        {
            std::lock_guard<std::mutex> lock(araQueueMutex);
            if (araQueue.empty())
                return;
            pending.swap(araQueue);
        }

        lattice::logDebug << "flushAraEvents: dispatching " << pending.size() << " event(s) to webview";
        for (const auto& ev : pending)
            sendWebViewMessage(ev);
    }

    // -------------------------------------------------------------------------
    // Framework internals — do not call directly
    // -------------------------------------------------------------------------

    // Called by the document controller to route an event into this instance's queue.
    void dispatchAraEvent(const nlohmann::json& event) { sendAraEvent(event); }

    // -------------------------------------------------------------------------
    // Source ownership helpers
    // -------------------------------------------------------------------------

    // Returns true if the given audio source is used by any playback region
    // assigned to this specific plugin instance. Checks PlaybackRenderer first,
    // then EditorRenderer, then EditorView region sequences — so it works even
    // when the host doesn't assign a PlaybackRenderer role to the UI instance.
    bool isMyAudioSource(ARA::PlugIn::AudioSource* source) const
    {
        if (source == nullptr)
            return false;

        // Helper: check a flat list of playback regions.
        auto sourceInRegions = [&](const std::vector<ARA::PlugIn::PlaybackRegion*>& regions) -> bool {
            for (const auto* region : regions)
                if (region->getAudioModification()->getAudioSource() == source)
                    return true;
            return false;
        };

        // PlaybackRenderer — the primary role for rendering instances.
        if (const auto* pr = araExtension.getPlaybackRenderer())
            if (!pr->getPlaybackRegions().empty())
                return sourceInRegions(pr->getPlaybackRegions());

        // EditorRenderer — has both getPlaybackRegions() and getRegionSequences();
        // check both since hosts may assign only one of the two.
        if (const auto* er = araExtension.getEditorRenderer())
        {
            if (!er->getPlaybackRegions().empty())
                return sourceInRegions(er->getPlaybackRegions());
            for (const auto* seq : er->getRegionSequences())
                for (const auto* region : seq->getPlaybackRegions())
                    if (region->getAudioModification()->getAudioSource() == source)
                        return true;
        }

        return false;
    }

    // Returns the ARA factory for this plugin type. Used by
    // LATTICE_DEFINE_ARA_FACTORY and wired into the getAraFactory callback.
    static const ARA::ARAFactory* getStaticAraFactory() noexcept
    {
        return ARA::PlugIn::PlugInEntry::getPlugInEntry<
            detail::LatticeFactoryConfig<Derived>,
            detail::LatticeDocumentController<Derived>>()->getFactory();
    }

    void onWebViewIsReady() override
    {
        // Only set the flag here. Do NOT call flushAraEvents() directly —
        // onWebViewIsReady() runs on the main thread while the idle thread
        // is already calling flushAraEvents() concurrently. Both paths call
        // sendWebViewMessage(), which writes to a SPSC queue; two concurrent
        // writers corrupt it and crash the WebKit process.
        // The idle thread's next flushAraEvents() cycle (≤16ms) will drain
        // any queued events now that webViewReady is true.
        webViewReady.store(true, std::memory_order_release);
    }

protected:
    AraProcessor() : Processor()
    {
        getAraFactory = []() -> const void* {
            return reinterpret_cast<const void*>(Derived::getStaticAraFactory());
        };

        bindToAraDocumentController = [this](void* dcRef, uint32_t known, uint32_t assigned) -> const void* {
            detail::currentBinding<Derived> = this; // captured by DC constructor on first bind
            const auto* inst = araExtension.bindToARA(
                reinterpret_cast<ARA::ARADocumentControllerRef>(dcRef),
                static_cast<ARA::ARAPlugInInstanceRoleFlags>(known),
                static_cast<ARA::ARAPlugInInstanceRoleFlags>(assigned));
            detail::currentBinding<Derived> = nullptr;

            // For the first bind the DC constructor already registered us via
            // currentBinding. For subsequent binds (DC already exists), register
            // explicitly so we receive broadcasts and the backlog.
            if (auto* dc = dynamic_cast<detail::LatticeDocumentController<Derived>*>(
                    araExtension.getDocumentController()))
                dc->addProcessor(this);

            return reinterpret_cast<const void*>(inst);
        };
    }

    // Exposed so subclasses can access playback renderer, editor renderer, etc.
    ARA::PlugIn::PlugInExtension araExtension;

private:
    std::mutex                 araQueueMutex;
    std::deque<nlohmann::json> araQueue;
    std::atomic<bool>          webViewReady{false};
};

} // namespace lattice

// ============================================================================
// LATTICE_DEFINE_ARA_FACTORY(ProcessorType)
//
// Defines the latticeGetAraFactory() free function required by FactoryImpl.cpp
// to register the ARA factory with the CLAP host. Place this exactly once in
// your plugin's .cpp file.
// ============================================================================
#define LATTICE_DEFINE_ARA_FACTORY(ProcessorType)                          \
    const ARA::ARAFactory* latticeGetAraFactory() noexcept {               \
        return ProcessorType::getStaticAraFactory();                        \
    }

#else // LATTICE_HAS_ARA == 0

// When the ARA SDK is not present the header is still safe to include.
// AraProcessor and AraPluginInfo are not defined; gate your code with
// #if LATTICE_HAS_ARA as needed.
namespace lattice { struct AraPluginInfo {}; }
#define LATTICE_DEFINE_ARA_FACTORY(ProcessorType)

#endif // LATTICE_HAS_ARA
