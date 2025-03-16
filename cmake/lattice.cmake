function(generate_plugin_header TARGET)
    set(oneValueArgs OUTPUT_DIRECTORY UNIQUE_ID NAME VENDOR URL MANUAL_URL SUPPORT_URL VERSION DESCRIPTION)
    set(multiValueArgs FEATURES)  # FEATURES can take multiple values

    cmake_parse_arguments(PLUGIN_INFO "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # Default values
    set(DEFAULT_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
    set(DEFAULT_UNIQUE_ID "default.plugin.id")
    set(DEFAULT_NAME "DefaultPlugin")
    set(DEFAULT_VENDOR "UnknownVendor")
    set(DEFAULT_URL "https://example.com")
    set(DEFAULT_MANUAL_URL "")
    set(DEFAULT_SUPPORT_URL "")
    set(DEFAULT_VERSION "1.0.0")
    set(DEFAULT_DESCRIPTION "Default plugin description")

    # Apply defaults if missing
    if(NOT PLUGIN_INFO_OUTPUT_DIRECTORY)
        set(PLUGIN_INFO_OUTPUT_DIRECTORY "${DEFAULT_OUTPUT_DIRECTORY}")
    endif()
    if(NOT PLUGIN_INFO_UNIQUE_ID)
        set(PLUGIN_INFO_UNIQUE_ID "${DEFAULT_UNIQUE_ID}")
    endif()
    if(NOT PLUGIN_INFO_NAME)
        set(PLUGIN_INFO_NAME "${DEFAULT_NAME}")
    endif()
    if(NOT PLUGIN_INFO_VENDOR)
        set(PLUGIN_INFO_VENDOR "${DEFAULT_VENDOR}")
    endif()
    if(NOT PLUGIN_INFO_URL)
        set(PLUGIN_INFO_URL "${DEFAULT_URL}")
    endif()
    if(NOT DEFINED PLUGIN_INFO_MANUAL_URL)
        set(PLUGIN_INFO_MANUAL_URL "${DEFAULT_MANUAL_URL}")
    endif()
    if(NOT DEFINED PLUGIN_INFO_SUPPORT_URL)
        set(PLUGIN_INFO_SUPPORT_URL "${DEFAULT_SUPPORT_URL}")
    endif()
    if(NOT PLUGIN_INFO_VERSION)
        set(PLUGIN_INFO_VERSION "${DEFAULT_VERSION}")
    endif()
    if(NOT PLUGIN_INFO_DESCRIPTION)
        set(PLUGIN_INFO_DESCRIPTION "${DEFAULT_DESCRIPTION}")
    endif()

    # Convert FEATURES list into a valid C++ array initializer format
    set(FEATURES_ARRAY "")
    foreach(FEATURE IN LISTS PLUGIN_INFO_FEATURES)
        set(FEATURES_ARRAY "${FEATURES_ARRAY}\"${FEATURE}\", ")
    endforeach()
    set(FEATURES_ARRAY "${FEATURES_ARRAY}nullptr")  # Ensure nullptr is last

    # Derive the header file name from the plugin name
    set(HEADER_NAME "${PLUGIN_INFO_NAME}Info.h")
    set(HEADER_FILE "${PLUGIN_INFO_OUTPUT_DIRECTORY}/${HEADER_NAME}")

    # Generate the header file
    file(WRITE ${HEADER_FILE} 
"#pragma once

#include <clap/clap.h>  // Ensure this is the correct CLAP header location

static constexpr const char* features[] = { ${FEATURES_ARRAY} };

static constexpr clap_plugin_descriptor descriptor = {
    .clap_version = CLAP_VERSION,
    .id = \"${PLUGIN_INFO_UNIQUE_ID}\",
    .name = \"${PLUGIN_INFO_NAME}\",
    .vendor = \"${PLUGIN_INFO_VENDOR}\",
    .url = \"${PLUGIN_INFO_URL}\",
    .manual_url = \"${PLUGIN_INFO_MANUAL_URL}\",
    .support_url = \"${PLUGIN_INFO_SUPPORT_URL}\",
    .version = \"${PLUGIN_INFO_VERSION}\",
    .description = \"${PLUGIN_INFO_DESCRIPTION}\",
    .features = features
};
")

    # Add the generated header to the target's include directories
    target_include_directories(${TARGET} PRIVATE ${PLUGIN_INFO_OUTPUT_DIRECTORY})

    # Define a preprocessor macro for the header name
    target_compile_definitions(${TARGET} PRIVATE 
        PLUGIN_INFO_HEADER="${HEADER_NAME}"
    )
endfunction()

# ==========================================================================
# Set teh lattice source files
set(LATTICE_SOURCE_FILES
    "${CMAKE_SOURCE_DIR}/src/lattice/LatticeProcessor.h"
    "${CMAKE_SOURCE_DIR}/src/lattice/LatticeServer.h"
    "${CMAKE_SOURCE_DIR}/src/lattice/LatticeServer.cpp"
    "${CMAKE_SOURCE_DIR}/src/lattice/LatticeUtils.h"
    "${CMAKE_SOURCE_DIR}/src/lattice/LatticeUtils.cpp"
    "${CMAKE_SOURCE_DIR}/src/lattice/clap/ClapPlugin.h"
    "${CMAKE_SOURCE_DIR}/src/lattice/clap/ClapPlugin.cpp"
    "${CMAKE_SOURCE_DIR}/src/lattice/clap/FactoryImpl.cpp"
)

# ======================================================================
# Set the Lattice include directories
set(LATTICE_INCLUDE_DIRS
    ${clap_SOURCE_DIR}/include
    ${choc_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${json_SOURCE_DIR}/include
    ${httplib_SOURCE_DIR}
)
