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
    GIT_TAG master  # Change to a specific tag like v0.13.0 for stability
)

# Make all dependencies available
FetchContent_MakeAvailable(clap clap-helpers clap-wrapper choc json httplib)
