//
// Created by Paul Walker on 2/23/25.
//

#include "FactoryImpl.h"
#include "../LatticeProcessor.h"
#include "LatticeClapPlugin.h"
#include "clapwrapper/auv2.h"
#include <iostream>
#include PLUGIN_INFO_HEADER  // Include the dynamically generated header

namespace impl
{
    bool init(const char* /*plugin_path*/)
    {        
        return true;
    }

    void deinit()
    {}

    uint32_t getPluginCount(const clap_plugin_factory* /*factory*/)
    {
        return 1;
    }

    const clap_plugin_descriptor* getPluginDescriptor(const clap_plugin_factory* /*factory*/, uint32_t index)
    {
        //the descriptor is defiend in CabbagePluginInfo.h
        if (index == 0)
            return &descriptor;

        return nullptr;
    }

    const clap_plugin* createPluginInstance(const clap_plugin_factory* /*factory*/, const clap_host* host, const char* plugin_id)
    {
        if (strcmp(plugin_id, descriptor.id))
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

    bool clap_get_auv2_info(const clap_plugin_factory_as_auv2* /*factory*/, uint32_t index,
                                   clap_plugin_info_as_auv2_t *info)
    {
        if (index > 1)
            return false;

        if (index == 0)
        {
            strncpy(info->au_type, "aufx", 5); // use the features to determine the type
            strncpy(info->au_subt, "CaBB", 5);
        }
        if (index == 1)
        {
            strncpy(info->au_type, "aufx", 5); // use the features to determine the type
            strncpy(info->au_subt, "CaBB", 5);
        }

        return true;
    }

    const void* getFactory(const char* factory_id)
    {
        if (strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID) == 0) {
            return &factoryStruct;
        }
        
        return nullptr;
    }

}
