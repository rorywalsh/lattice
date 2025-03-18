cmake_minimum_required(VERSION 3.25)

# ----------------------------
# Project Configuration
# ----------------------------
project(LatticePlugins LANGUAGES CXX VERSION 0.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CLAP_WRAPPER_DONT_ADD_TARGETS TRUE)
set(CLAP_WRAPPER_DOWNLOAD_DEPENDENCIES TRUE CACHE BOOL "Download Dependencies")
option(COPY_AFTER_BUILD "Install local copy after build" FALSE)

# ----------------------------
# Apple-Specific Settings
# ----------------------------
if (APPLE)
    enable_language(OBJC)
    enable_language(OBJCXX)
    
    set(CMAKE_OBJC_VISIBILITY_PRESET hidden)
    set(CMAKE_OBJCXX_VISIBILITY_PRESET hidden)
    set(CMAKE_OBJCXX_FLAGS "${CMAKE_OBJCXX_FLAGS} -fobjc-arc")

    # Link against the Cocoa framework
    find_library(COCOA_LIBRARY Cocoa)
endif()

# ----------------------------
# Dependencies
# ----------------------------



include(cmake/Dependencies.cmake)

# Set LATTICE_INCLUDE_DIRS, and LATTICE_SOURCE_FILES
# Also defines the generate_plugin_header function
include(cmake/lattice.cmake)

include_directories(
    ${clap_SOURCE_DIR}/include
    ${clap-helpers_SOURCE_DIR}/include
)

# ----------------------------
# Add Plugin Directories
# ----------------------------
add_subdirectory(src/examples/gain)
add_subdirectory(src/examples/flanger)
add_subdirectory(src/examples/multichannel)