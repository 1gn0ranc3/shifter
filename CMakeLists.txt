cmake_minimum_required(VERSION 3.22)

project(Shifter
    VERSION 0.1.0
    DESCRIPTION "Low-latency pitch shifter VST3 plugin"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version" FORCE)
endif()

option(SHIFTER_BUILD_TESTS "Build unit tests" ON)
option(SHIFTER_COPY_AFTER_BUILD "Install VST3 into the system plug-in folder after build" ON)

include(FetchContent)

# ---- JUCE 8 ------------------------------------------------------------------
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.15
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

# ---- Catch2 (tests only) -----------------------------------------------------
if(SHIFTER_BUILD_TESTS)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.5.4
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
    enable_testing()
endif()

# ---- Project targets ---------------------------------------------------------
add_subdirectory(src)

if(SHIFTER_BUILD_TESTS)
    add_subdirectory(tests)
endif()
