include(FetchContent)

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

FetchContent_MakeAvailable(choc json)
