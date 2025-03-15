set(PLUGIN_NAME GainPlugin)

set(SOURCE_FILES
    GainProcessor.cpp
    GainProcessor.h
    ${CMAKE_SOURCE_DIR}/src/lattice/clap/Factory.cpp
)

add_library(${PLUGIN_NAME}-impl STATIC ${SOURCE_FILES})

# Generate CabbagePluginInfo.h header
generatePluginInfoHeader(${PLUGIN_NAME}-impl
    UNIQUE_ID "your.reversed.domain.name.GainPlugin"
    NAME "GainPlugin"
    VENDOR "CabbageAudio"
    URL "https://cabbageaudio.com"
    MANUAL_URL "https://docs.cabbageaudio.com"
    SUPPORT_URL "https://support.cabbageaudio.com"
    VERSION "1.0.0"
    DESCRIPTION "Gain Plugin"
    FEATURES 
        CLAP_PLUGIN_FEATURE_AUDIO_EFFECT 
        CLAP_PLUGIN_FEATURE_UTILITY 
        CLAP_PLUGIN_FEATURE_STEREO
)

# Include Paths
target_include_directories(${PLUGIN_NAME}-impl PRIVATE 
    ${clap_SOURCE_DIR}/include
    ${choc_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${json_SOURCE_DIR}/include
    ${httplib_SOURCE_DIR}
)

# Compiler and Platform-Specific Options
if (WIN32)
    target_compile_definitions(${PLUGIN_NAME}-impl PRIVATE _CRT_SECURE_NO_WARNINGS)    
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded" CACHE STRING "Multithreaded")
else()
    target_compile_options(${PLUGIN_NAME}-impl PRIVATE -Wall -Wextra -pedantic)
endif()

# Linking
if (APPLE)
    target_link_libraries(${PLUGIN_NAME}-impl PUBLIC clap clap-helpers ${COCOA_LIBRARY})
endif()

# Plugin Build Configuration
make_clapfirst_plugins(
    TARGET_NAME ${PLUGIN_NAME}
    IMPL_TARGET ${PLUGIN_NAME}-impl
    OUTPUT_NAME "${PLUGIN_NAME}"
    ENTRY_SOURCE ${CMAKE_SOURCE_DIR}/src/lattice/clap/Factory.cpp
    BUNDLE_IDENTIFER "org.yourproject.${PLUGIN_NAME}"
    BUNDLE_VERSION ${PROJECT_VERSION}
    COPY_AFTER_BUILD ${COPY_AFTER_BUILD}
    PLUGIN_FORMATS CLAP VST3 # AUV2
    ASSET_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PLUGIN_NAME}_assets
)

# Plugin-Specific Resource Copying
set(PLUGIN_BUNDLE_DIR ${CMAKE_BINARY_DIR}/${PLUGIN_NAME}_assets/Debug/${PLUGIN_NAME}.vst3)
set(RESOURCE_DIR ${PLUGIN_BUNDLE_DIR}/Contents/Resources)
set(SOURCE_RESOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/html)  # Plugin-specific html folder

# Define a custom target that performs the post-build actions
add_custom_target(${PLUGIN_NAME}_post_build
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PLUGIN_BUNDLE_DIR}  # Create the .vst3 directory
    COMMAND ${CMAKE_COMMAND} -E make_directory ${RESOURCE_DIR}         # Create the Resources directory
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${SOURCE_RESOURCE_DIR} ${RESOURCE_DIR}  # Copy the contents
    COMMENT "Copying ${PLUGIN_NAME} resources to plugin bundle"
)

# Ensure the post-build actions are triggered after the plugin is built
add_dependencies(${PLUGIN_NAME}-impl ${PLUGIN_NAME}_post_build)
add_dependencies(${PLUGIN_NAME}_vst3 ${PLUGIN_NAME}_post_build)
add_dependencies(${PLUGIN_NAME}_clap ${PLUGIN_NAME}_post_build)