//
// Created by Paul Walker on 2/23/25.
//

#include "FactoryImpl.h"
#include "../LatticeProcessor.h"
#include "LatticeClapPlugin.h"
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

    const void* getFactory(const char* factory_id)
    {
      if (strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID) == 0) {
        return &factoryStruct;
        }
        return nullptr;
    }

}
