//
// Created by Paul Walker on 2/23/25.
//

#include "FactoryImpl.h"
#include "../LatticeProcessor.h"
#include "../LatticeUtils.h"
#include "LatticeClapPlugin.h"
#include "clapwrapper/auv2.h"
#include <iostream>
#include PLUGIN_INFO_HEADER  // Include the dynamically generated header

#if __has_include(<ARA_API/ARACLAP.h>)
#include <ARA_API/ARACLAP.h>
#define LATTICE_HAS_ARA_CLAP 1
#else
#define LATTICE_HAS_ARA_CLAP 0
#endif

#if LATTICE_HAS_ARA_CLAP && defined(LATTICE_PLUGIN_PROVIDES_ARA_FACTORY)
extern const ARA::ARAFactory* latticeGetAraFactory();
#endif

namespace impl
{
    bool init(const char* plugin_path)
    {
        if (plugin_path && std::strlen(plugin_path) > 0)
            lattice::File::setBinaryFilePathOverride(plugin_path);
        return true;
    }

    void deinit()
    {}

    uint32_t getPluginCount(const clap_plugin_factory* /*factory*/)
    {
        // Only supports single plugin libraries for now
        return 1;
    }

    const clap_plugin_descriptor* getPluginDescriptor(const clap_plugin_factory* /*factory*/, uint32_t index)
    {
        // The descriptor is defiend in PLUGIN_INFO_HEADER which is set via Cmake
        if (index == 0)
            return getDescriptor();

        return nullptr;
    }

    const clap_plugin* createPluginInstance(const clap_plugin_factory* /*factory*/, const clap_host* host, const char* plugin_id)
    {
        if (strcmp(plugin_id, getDescriptor()->id))
        {
            std::cerr << "Error: plugin_id '" << plugin_id << "' not found!" << std::endl;
            return nullptr;
        }

        // Host will own 'plugin'
        auto plugin = LatticeProcessorPluginFactory::createPlugin(host); // No need for user to specify inputs/outputs
                
        return plugin->clapPlugin();
    }

    const clap_plugin_factory factoryStruct =
    {
        .get_plugin_count = getPluginCount,
        .get_plugin_descriptor = getPluginDescriptor,
        .create_plugin = createPluginInstance,
    };

#if LATTICE_HAS_ARA_CLAP
    uint32_t getAraFactoryCount(const clap_ara_factory* /*factory*/)
    {
#if defined(LATTICE_PLUGIN_PROVIDES_ARA_FACTORY)
        return (latticeGetAraFactory() != nullptr) ? 1u : 0u;
#else
        return 0u;
#endif
    }

    const ARA::ARAFactory* getAraFactory(const clap_ara_factory* /*factory*/, uint32_t index)
    {
#if defined(LATTICE_PLUGIN_PROVIDES_ARA_FACTORY)
        if (index == 0)
            return latticeGetAraFactory();
#endif
        return nullptr;
    }

    const char* getAraPluginId(const clap_ara_factory* /*factory*/, uint32_t /*index*/)
    {
        return getDescriptor()->id;
    }

    const clap_ara_factory araFactoryStruct =
    {
        .get_factory_count = getAraFactoryCount,
        .get_ara_factory = getAraFactory,
        .get_plugin_id = getAraPluginId,
    };
#endif

    bool clap_get_auv2_info(const clap_plugin_factory_as_auv2* /*factory*/, uint32_t index,
                                   clap_plugin_info_as_auv2_t *info)
    {
        if (index > 1)
            return false;

        strncpy(info->au_type, LATTICE_AU_TYPE, 5); // use the features to determine the type
        strncpy(info->au_subt, LATTICE_AU_SUBTYPE, 5);



        return true;
    }

    const void* getFactory(const char* factory_id)
    {
        if (strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID) == 0) {
            return &factoryStruct;
        }

#if LATTICE_HAS_ARA_CLAP
        if (strcmp(factory_id, CLAP_EXT_ARA_FACTORY) == 0)
        {
            return &araFactoryStruct;
        }
#endif
        
        if (strcmp(factory_id, CLAP_PLUGIN_FACTORY_INFO_AUV2) == 0)
        {
            static const struct clap_plugin_factory_as_auv2 auv2_factory = {
                LATTICE_MANUFACTURER_CODE,      // manu code
                LATTICE_MANUFACTURER_NAME, // manu name
                clap_get_auv2_info};
            return &auv2_factory;
        }
        
        return nullptr;
    }

}
