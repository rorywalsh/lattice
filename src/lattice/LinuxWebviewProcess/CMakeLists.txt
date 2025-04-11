cmake_minimum_required(VERSION 3.14)
project(WebViewProcess)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK REQUIRED gtk+-3.0 webkit2gtk-4.1)

include(cmake/Dependencies.cmake)

# Include directories
include_directories(
    ${GTK_INCLUDE_DIRS}
    ${choc_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${json_SOURCE_DIR}/include
)



set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/webview_output)

# Define the executable
add_executable(WebViewProcess WebviewProcess.cpp)

# Link libraries
target_link_libraries(WebViewProcess
    ${GTK_LIBRARIES}
    X11
    Xext
)

# Setting this to allow me to link to the this target from other targets
set_property(TARGET WebViewProcess PROPERTY ENABLE_EXPORTS ON)

# Compiler flags
target_compile_options(WebViewProcess PRIVATE ${GTK_CFLAGS_OTHER})

# Define a custom array name
set(BINARY_ARRAY_NAME "webview_binary")

# Custom command to generate a header file using xxd with a custom array name
add_custom_command(
    TARGET WebViewProcess
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/generated

    # Execute the bash script with arguments
    COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/generate_header.sh
        "$<TARGET_FILE:WebViewProcess>"
        "${CMAKE_SOURCE_DIR}/webview_binary.h"
        "${BINARY_ARRAY_NAME}"

    COMMENT "Generating Base64-encoded header file"
)


# Optionally, add the generated directory to the include path
target_include_directories(WebViewProcess PUBLIC ${CMAKE_BINARY_DIR}/generated)