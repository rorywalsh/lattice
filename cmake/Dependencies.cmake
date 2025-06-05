include(FetchContent)

# Declare other dependencies
FetchContent_Declare(
    clap
    GIT_REPOSITORY https://github.com/free-audio/clap.git
    GIT_TAG main
    FIND_PACKAGE_ARGS NAMES clap
)

FetchContent_Declare(
    clap-wrapper
    GIT_REPOSITORY https://github.com/free-audio/clap-wrapper
    GIT_TAG main
    FIND_PACKAGE_ARGS NAMES clap-wrapper
)

FetchContent_Declare(
    clap-helpers
    GIT_REPOSITORY https://github.com/free-audio/clap-helpers.git
    GIT_TAG main
    FIND_PACKAGE_ARGS NAMES clap-helpers
)

FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG master
)

FetchContent_Declare(
    choc
    GIT_REPOSITORY https://github.com/Tracktion/choc
    GIT_TAG 1330e77172bcce2e0e752b98a76f49c5062cf3aa
    FIND_PACKAGE_ARGS NAMES choc
)

FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG master  
)

FetchContent_Declare(
    aurora
    GIT_REPOSITORY https://github.com/vlazzarini/aurora.git
    GIT_TAG main  
)

FetchContent_Declare(
    cppcodec
    GIT_REPOSITORY https://github.com/tplgy/cppcodec.git
    GIT_TAG        master
)

# Make all dependencies available except aurora
FetchContent_MakeAvailable(clap clap-helpers clap-wrapper choc json httplib cppcodec)

# Enable OLD behavior for FetchContent_Populate to silence the warning
cmake_policy(SET CMP0169 OLD)

# Fetch aurora but do not add it to the build
FetchContent_GetProperties(aurora)
if(NOT aurora_POPULATED)
    FetchContent_Populate(aurora)
    # Do NOT call add_subdirectory for aurora
endif()

# Reset policy to default (optional, but good practice)
cmake_policy(SET CMP0169 NEW)
