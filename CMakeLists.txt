cmake_minimum_required(VERSION 3.25)

# ----------------------------
# Project Configuration
# ----------------------------
project(LatticePlugins LANGUAGES C CXX VERSION 0.1)

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
option(LATTICE_ENABLE_ARA "Enable ARA scaffolding in Lattice" OFF)
option(LATTICE_FETCH_ARA_SDK "Fetch ARA_SDK when LATTICE_ENABLE_ARA is ON and no local SDK is found" OFF)
set(LATTICE_ARA_SDK_DIR "" CACHE PATH "Path to ARA SDK root containing ARA_API")

if(LATTICE_ENABLE_ARA)
  if(NOT LATTICE_ARA_SDK_DIR)
    get_filename_component(_LATTICE_VENDORED_ARA_DIR "${CMAKE_SOURCE_DIR}/external/ARA_SDK" ABSOLUTE)
    get_filename_component(_LATTICE_WORKSPACE_ARA_DIR "${CMAKE_SOURCE_DIR}/../ARA_SDK" ABSOLUTE)

    if(EXISTS "${_LATTICE_VENDORED_ARA_DIR}/ARA_API")
      set(LATTICE_ARA_SDK_DIR "${_LATTICE_VENDORED_ARA_DIR}" CACHE PATH "Path to ARA SDK root containing ARA_API" FORCE)
    elseif(EXISTS "${_LATTICE_WORKSPACE_ARA_DIR}/ARA_API")
      set(LATTICE_ARA_SDK_DIR "${_LATTICE_WORKSPACE_ARA_DIR}" CACHE PATH "Path to ARA SDK root containing ARA_API" FORCE)
    endif()
  endif()

  if((NOT LATTICE_ARA_SDK_DIR OR NOT EXISTS "${LATTICE_ARA_SDK_DIR}/ARA_API") AND LATTICE_FETCH_ARA_SDK)
    include(FetchContent)
    FetchContent_Declare(
      ara_sdk
      GIT_REPOSITORY https://github.com/Celemony/ARA_SDK.git
      GIT_TAG master
    )
    FetchContent_MakeAvailable(ara_sdk)
    if(EXISTS "${ara_sdk_SOURCE_DIR}/ARA_API")
      set(LATTICE_ARA_SDK_DIR "${ara_sdk_SOURCE_DIR}" CACHE PATH "Path to ARA SDK root containing ARA_API" FORCE)
    endif()
  endif()

  if(NOT LATTICE_ARA_SDK_DIR OR NOT EXISTS "${LATTICE_ARA_SDK_DIR}/ARA_API")
    message(FATAL_ERROR "LATTICE_ENABLE_ARA is ON, but no valid ARA SDK was found. Set LATTICE_ARA_SDK_DIR or enable LATTICE_FETCH_ARA_SDK.")
  endif()
endif()

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
if(LATTICE_ENABLE_ARA)
add_subdirectory(examples/GainARA)
endif()
add_subdirectory(examples/Flanger)
add_subdirectory(examples/MultiChannel)
add_subdirectory(examples/SimpleSynth)
endif()
