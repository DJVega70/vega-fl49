cmake_minimum_required(VERSION 3.22)
project(VegaDAW VERSION 0.4.0)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")
if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Tracktion Engine (GPLv3) + its bundled JUCE. Pass -DTE_DIR=<path to a tracktion_engine checkout>.
if (NOT DEFINED TE_DIR)
    message(FATAL_ERROR "Set TE_DIR to a tracktion_engine checkout (with its modules/juce submodule)")
endif()
add_subdirectory(${TE_DIR}/modules/juce juce_build)
add_subdirectory(${TE_DIR}/modules tracktion_build)

option(VEGA_ASIO "Build with ASIO support (needs ASIO_SDK_DIR)" OFF)
# VEGA FL49: the edition built around the Novation FLkey 2 49 (same engine and features, plus the FLkey panel,
# scale / chord keys and pad / knob integration). -DVEGA_FL49=ON builds "VEGA FL49.exe" instead of "Vega Productions DAW.exe".
option(VEGA_FL49 "Build VEGA FL49 (OFF = the plain Vega Productions DAW shell)" ON)
if (VEGA_FL49)
    set(VEGA_PRODUCT "VEGA FL49")
    set(VEGA_BUNDLE com.vegaproductions.fl49)
else()
    set(VEGA_PRODUCT "Vega Productions DAW")
    set(VEGA_BUNDLE com.vegaproductions.daw)
endif()

juce_add_gui_app(VegaDAW
    PRODUCT_NAME "${VEGA_PRODUCT}"
    COMPANY_NAME "Vega Productions"
    BUNDLE_ID ${VEGA_BUNDLE}
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Resources/VegaDAW-icon.png"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Recording audio"
    NEEDS_CURL FALSE
    NEEDS_WEB_BROWSER FALSE
    PLUGINHOST_AU TRUE
    DOCUMENT_EXTENSIONS vegaproj)

target_sources(VegaDAW PRIVATE
    Source/Main.cpp
    Source/DawSession.cpp
    Source/ArrangeView.cpp
    Source/PianoRoll.cpp
    Source/MixerView.cpp
    Source/HuiSurface.cpp
    Source/ApcSurface.cpp
    Source/Controllers.cpp
    Source/Fl49Hub.cpp)

target_compile_definitions(VegaDAW PRIVATE
    JUCE_PLUGINHOST_AU=1
    JUCE_PLUGINHOST_VST3=1
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_APPLICATION_NAME_STRING="${VEGA_PRODUCT}"
    VEGA_FL49=$<BOOL:${VEGA_FL49}>
    JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}"
    TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH=1
    TRACKTION_ENABLE_CONTROL_SURFACES=1)

if (VEGA_ASIO AND WIN32)
    target_include_directories(VegaDAW PRIVATE ${ASIO_SDK_DIR}/common)
    target_compile_definitions(VegaDAW PRIVATE JUCE_ASIO=1)
endif()

target_link_libraries(VegaDAW PRIVATE
    tracktion::tracktion_core
    tracktion::tracktion_engine
    tracktion::tracktion_graph
    juce::juce_audio_devices
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_recommended_config_flags)

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    target_compile_options(VegaDAW PRIVATE "-fno-aligned-allocation")
endif()
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(VegaDAW PRIVATE "-latomic")
endif()


# ---- AI stem separation: demucs.cpp (MIT, Demucs v4 inference in C++) + Eigen 3.4 (MPL-2.0)
# Weights are NOT bundled; the app downloads them on first use.
include(FetchContent)
if (DEFINED DEMUCS_DIR)
    set(demucs_SOURCE_DIR ${DEMUCS_DIR})
else()
    FetchContent_Declare(demucs GIT_REPOSITORY https://github.com/sevagh/demucs.cpp GIT_TAG 4edae497a114de1ccb17ac9b4a2b8fc0144714d7
                         GIT_SUBMODULES "" SOURCE_SUBDIR do-not-add)
    FetchContent_MakeAvailable(demucs)
endif()
if (DEFINED EIGEN_DIR)
    set(VEGA_EIGEN_INCLUDE ${EIGEN_DIR})
else()
    # the Eigen commit demucs.cpp is developed against (3.4.0 lacks Tensor fixes it relies on)
    FetchContent_Declare(eigen URL https://gitlab.com/libeigen/eigen/-/archive/dd8c71e62852b2fe429edb6682ac91fd1c578a26/eigen-dd8c71e62852b2fe429edb6682ac91fd1c578a26.tar.gz SOURCE_SUBDIR do-not-add)
    FetchContent_MakeAvailable(eigen)
    set(VEGA_EIGEN_INCLUDE ${eigen_SOURCE_DIR})
endif()
file(GLOB DEMUCS_SOURCES ${demucs_SOURCE_DIR}/src/*.cpp)

# The separation runs in a helper program (VegaStemHelper), not inside the DAW: a crash or out-of-memory
# there can't take the session down. On Windows there are two builds - AVX2 (fast) and a generic one for
# any x64 CPU. Every file that touches Eigen in a helper is compiled with exactly the same flags, so all of
# them agree on Eigen's memory alignment (mixing AVX2 and non-AVX2 Eigen code crashes).
function(vega_stem_variant suffix arch)
    set(lib vega_demucs${suffix})
    add_library(${lib} STATIC ${DEMUCS_SOURCES})
    target_include_directories(${lib} PUBLIC ${demucs_SOURCE_DIR}/src ${VEGA_EIGEN_INCLUDE})
    target_compile_features(${lib} PUBLIC cxx_std_17)
    target_compile_definitions(${lib} PUBLIC NDEBUG EIGEN_NO_DEBUG _USE_MATH_DEFINES)
    set_target_properties(${lib} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    if (MSVC)
        target_compile_options(${lib} PUBLIC /O2 /bigobj /EHsc /W0 ${arch})
    else()
        target_compile_options(${lib} PUBLIC -O3 -w ${arch})
    endif()
    set(VEGA_DEMUCS_LIB_${suffix} ${lib} PARENT_SCOPE)
endfunction()
vega_stem_variant("" "")
if (MSVC)
    vega_stem_variant("_avx2" "/arch:AVX2")
endif()

function(vega_stem_helper target product lib)
    juce_add_console_app(${target} PRODUCT_NAME "${product}")
    target_sources(${target} PRIVATE Source/StemHelper.cpp Source/StemSeparator.cpp)
    target_compile_definitions(${target} PRIVATE JUCE_USE_MP3AUDIOFORMAT=1 JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
    target_link_libraries(${target} PRIVATE ${lib} juce::juce_audio_formats juce::juce_core)
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_libraries(${target} PRIVATE "-latomic")
    endif()
    add_dependencies(VegaDAW ${target})
    # put the helper next to the DAW executable
    add_custom_command(TARGET ${target} POST_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:VegaDAW>"
                       COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${target}>" "$<TARGET_FILE_DIR:VegaDAW>")
endfunction()
vega_stem_helper(VegaStemHelper VegaStemHelper vega_demucs)
if (MSVC)
    vega_stem_helper(VegaStemHelperAVX2 VegaStemHelperAVX2 vega_demucs_avx2)
endif()

# headless self-test (engine round trips: tracks, clips, routing, render)
option(VEGA_TESTS "Build the headless test app" OFF)
if (VEGA_TESTS)
    juce_add_console_app(VegaDAWTest PRODUCT_NAME "VegaDAWTest")
    target_sources(VegaDAWTest PRIVATE tests/DawTest.cpp Source/DawSession.cpp Source/HuiSurface.cpp Source/ApcSurface.cpp Source/StemSeparator.cpp Source/Fl49Hub.cpp)
    target_link_libraries(VegaDAWTest PRIVATE vega_demucs)
    target_compile_definitions(VegaDAWTest PRIVATE
        JUCE_PLUGINHOST_AU=1 JUCE_PLUGINHOST_VST3=1 JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0 TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH=1 TRACKTION_ENABLE_CONTROL_SURFACES=1 VEGA_HEADLESS=1)
    target_link_libraries(VegaDAWTest PRIVATE tracktion::tracktion_core tracktion::tracktion_engine tracktion::tracktion_graph
        juce::juce_audio_devices juce::juce_audio_processors juce::juce_audio_utils juce::juce_gui_extra juce::juce_recommended_config_flags)
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_libraries(VegaDAWTest PRIVATE "-latomic")
    endif()
endif()
