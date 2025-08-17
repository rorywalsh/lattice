set(PLUGIN_NAME GainPlugin)
set(CLAP_WRAPPER_OUTPUT_NAME ${PLUGIN_NAME})
set(LATTICE_ROOT_DIR "${CMAKE_SOURCE_DIR}")

set(SOURCE_FILES
    GainProcessor.cpp
    GainProcessor.h
    ${LATTICE_SOURCE_FILES} # Include lattice source files
)

add_library(${PLUGIN_NAME}-impl STATIC ${SOURCE_FILES})

# Generate plugin header - this will define the project properties
generate_plugin_header(${PLUGIN_NAME}-impl
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}  # Output directory for the header
    UNIQUE_ID "your.reversed.domain.name.${PLUGIN_NAME}"
    NAME "${PLUGIN_NAME}"
    VENDOR "CabbageAudio"
    URL "https://cabbageaudio.com"
    MANUAL_URL "https://docs.cabbageaudio.com"
    SUPPORT_URL "https://support.cabbageaudio.com"
    VERSION "1.0.0"
    DESCRIPTION "${PLUGIN_NAME} Plugin"
    FEATURES 
        CLAP_PLUGIN_FEATURE_AUDIO_EFFECT 
        CLAP_PLUGIN_FEATURE_UTILITY 
        CLAP_PLUGIN_FEATURE_STEREO
)

# Include Paths
target_include_directories(${PLUGIN_NAME}-impl PRIVATE 
    ${LATTICE_INCLUDE_DIRS}
)

# Define a preprocessor macro for the header name
target_compile_definitions(${PLUGIN_NAME}-impl PRIVATE 
    PLUGIN_INFO_HEADER="${PLUGIN_NAME}Info.h"
    PRODUCT_NAME="${PLUGIN_NAME}"
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
    ENTRY_SOURCE ${LATTICE_ROOT_DIR}/src/lattice/clap/Factory.cpp
    BUNDLE_IDENTIFER "org.yourproject.${PLUGIN_NAME}"
    BUNDLE_VERSION ${PROJECT_VERSION}
    COPY_AFTER_BUILD ${COPY_AFTER_BUILD}
    PLUGIN_FORMATS CLAP VST3 AUV2
    ASSET_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PLUGIN_NAME}_assets
    STANDALONE_CONFIGURATIONS
        standalone "${PLUGIN_NAME}" "your.reversed.domain.name.${PLUGIN_NAME}"
)


set(SOURCE_RESOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/html")
COPY_PLUGIN_RESOURCES(${PLUGIN_NAME} ${SOURCE_RESOURCE_DIR})