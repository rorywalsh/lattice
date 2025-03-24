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
# Set the lattice source files
set(LATTICE_SOURCE_FILES
    "${LATTICE_ROOT_DIR}/src/lattice/LatticeProcessor.h"
    "${LATTICE_ROOT_DIR}/src/lattice/LatticeStructs.h"
    "${LATTICE_ROOT_DIR}/src/lattice/LatticeServer.h"
    "${LATTICE_ROOT_DIR}/src/lattice/LatticeServer.cpp"
    "${LATTICE_ROOT_DIR}/src/lattice/LatticeUtils.h"
    "${LATTICE_ROOT_DIR}/src/lattice/LatticeUtils.cpp"
    "${LATTICE_ROOT_DIR}/src/lattice/clap/ClapPlugin.h"
    "${LATTICE_ROOT_DIR}/src/lattice/clap/ClapPlugin.cpp"
    "${LATTICE_ROOT_DIR}/src/lattice/clap/FactoryImpl.cpp"
)

# Apple-Specific Sources
if (APPLE)    
    list(APPEND LATTICE_SOURCE_FILES "${LATTICE_ROOT_DIR}/src/lattice/clap/platform/MacParent.mm")
endif()

# ======================================================================
# Set the Lattice include directories
set(LATTICE_INCLUDE_DIRS
    ${clap_SOURCE_DIR}/include
    ${choc_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${json_SOURCE_DIR}/include
    ${httplib_SOURCE_DIR}
)

# ======================================================================
function(COPY_PLUGIN_RESOURCES PLUGIN_NAME SOURCE_RESOURCE_DIR)
    # Define the output directories for each plugin type
    if (WIN32)
        set(PLUGIN_BUNDLE_DIR_STANDALONE ${CMAKE_BINARY_DIR}/${PLUGIN_NAME}_assets/Standalone-${PLUGIN_NAME}_standalone/Debug)
        set(PLUGIN_BUNDLE_DIR_VST3 ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/${PLUGIN_NAME}.vst3)
        set(PLUGIN_BUNDLE_DIR_CLAP ${CMAKE_BINARY_DIR}/${PLUGIN_NAME}_assets/${PLUGIN_NAME}.clap)
    else()
        set(PLUGIN_BUNDLE_DIR_STANDALONE ${CMAKE_BINARY_DIR}/${PLUGIN_NAME}_assets/${CMAKE_CFG_INTDIR}/${PLUGIN_NAME}.app)   
        set(PLUGIN_BUNDLE_DIR_VST3 ${CMAKE_BINARY_DIR}/${PLUGIN_NAME}_assets/${CMAKE_CFG_INTDIR}/${PLUGIN_NAME}.vst3)
        set(PLUGIN_BUNDLE_DIR_CLAP ${CMAKE_BINARY_DIR}/${PLUGIN_NAME}_assets/${CMAKE_CFG_INTDIR}/${PLUGIN_NAME}.clap)
    endif()

    # Resource directories for each plugin type
    set(RESOURCE_DIR_VST3 ${PLUGIN_BUNDLE_DIR_VST3}/Contents/Resources)
    set(RESOURCE_DIR_CLAP ${PLUGIN_BUNDLE_DIR_CLAP}/Contents/Resources)

    if(TARGET ${PLUGIN_NAME}_standalone)
        if (WIN32)
            set(RESOURCE_DIR_STANDALONE ${PLUGIN_BUNDLE_DIR_STANDALONE}/Resources)
        else()
            set(RESOURCE_DIR_STANDALONE ${PLUGIN_BUNDLE_DIR_STANDALONE}/Contents/Resources)
        endif()
    endif()


    # Create a custom target for post-build actions
    add_custom_target(${PLUGIN_NAME}_post_build
        COMMAND ${CMAKE_COMMAND} -E make_directory ${PLUGIN_BUNDLE_DIR_VST3}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${RESOURCE_DIR_VST3}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${SOURCE_RESOURCE_DIR} ${RESOURCE_DIR_VST3}

        COMMAND ${CMAKE_COMMAND} -E make_directory ${PLUGIN_BUNDLE_DIR_CLAP}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${RESOURCE_DIR_CLAP}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${SOURCE_RESOURCE_DIR} ${RESOURCE_DIR_CLAP}

        COMMAND ${CMAKE_COMMAND} -E make_directory ${PLUGIN_BUNDLE_DIR_STANDALONE}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${RESOURCE_DIR_STANDALONE}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${SOURCE_RESOURCE_DIR} ${RESOURCE_DIR_STANDALONE}

        COMMENT "Copying ${PLUGIN_NAME} resources to plugin bundles"
    )

    # Ensure the post-build actions are triggered after the plugin is built
    add_dependencies(${PLUGIN_NAME}-impl ${PLUGIN_NAME}_post_build)
    add_dependencies(${PLUGIN_NAME}_vst3 ${PLUGIN_NAME}_post_build)
    add_dependencies(${PLUGIN_NAME}_clap ${PLUGIN_NAME}_post_build)

    # Only add the standalone dependency if the target exists
    if(TARGET ${PLUGIN_NAME}_standalone)
        add_dependencies(${PLUGIN_NAME}_standalone ${PLUGIN_NAME}_post_build)
    endif()
endfunction()
