cmake_minimum_required(VERSION 3.25)

# ----------------------------
# Project Configuration
# ----------------------------
project(LatticePlugins LANGUAGES CXX VERSION 0.1)

if (NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
  if (WIN32)
    message(STATUS "CMAKE_MSVC_RUNTIME_LIBRARY not defined. Setting to static link")
  endif()
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>") # Statically link the MSVC Runtime
endif()


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CLAP_WRAPPER_DONT_ADD_TARGETS TRUE)
set(CLAP_WRAPPER_DOWNLOAD_DEPENDENCIES TRUE CACHE BOOL "Download Dependencies")
option(COPY_AFTER_BUILD "Install local copy after build" FALSE)
option(LATTICE_BUILD_EXAMPLES "Build all examples" TRUE)

set(CLAP_WRAPPER_WINDOWS_SINGLE_FILE OFF)
# Enabled vst3 bundle folder on Windows
set(V3_WINDOWS_FOLDER_VST3  TRUE CACHE BOOL "")

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


set(LATTICE_ROOT_DIR "${CMAKE_SOURCE_DIR}")
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
if(LATTICE_BUILD_EXAMPLES)
add_subdirectory(examples/Gain)
add_subdirectory(examples/Flanger)
add_subdirectory(examples/MultiChannel)
add_subdirectory(examples/SimpleSynth)
endif()
