cmake_minimum_required(VERSION 3.25)

# ----------------------------
# Project Configuration
# ----------------------------
project(LatticePlugins LANGUAGES CXX VERSION 0.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(COPY_AFTER_BUILD "Install local copy after build" TRUE)
option(CABBAGE_SERVICE_APP "Build the Cabbage service application instead of the plugin" OFF)

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
set(CLAP_WRAPPER_DOWNLOAD_DEPENDENCIES TRUE CACHE BOOL "Download Dependencies")
set(CLAP_WRAPPER_OUTPUT_NAME "${PROJECT_NAME}")
set(CLAP_WRAPPER_DONT_ADD_TARGETS TRUE)

include(cmake/Dependencies.cmake)
include(cmake/lattice.cmake)

include_directories(
    ${clap_SOURCE_DIR}/include
    ${clap-helpers_SOURCE_DIR}/include
)

# ----------------------------
# Add Plugin Directories
# ----------------------------
add_subdirectory(src/examples/gain)