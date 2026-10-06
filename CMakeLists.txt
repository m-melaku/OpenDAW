cmake_minimum_required(VERSION 3.22)

project(OpenDAW VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static MSVC runtime so the .exe runs without installing VC++ redistributables
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Tracktion Engine also pulls in its bundled JUCE (external/tracktion_engine/modules/juce)
set(TE_ADD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(external/tracktion_engine)

set_property(GLOBAL PROPERTY USE_FOLDERS YES)

juce_add_gui_app(OpenDAW
    PRODUCT_NAME "OpenDAW"
    COMPANY_NAME "OpenDAW"
    VERSION ${PROJECT_VERSION}
    NEEDS_CURL FALSE
    NEEDS_WEB_BROWSER FALSE)

juce_generate_juce_header(OpenDAW)

target_sources(OpenDAW PRIVATE
    src/ArrangementView.cpp
    src/ArrangementView.h
    src/Main.cpp
    src/MainComponent.cpp
    src/MainComponent.h
    src/PianoRoll.cpp
    src/PianoRoll.h)

target_compile_definitions(OpenDAW PRIVATE
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:OpenDAW,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:OpenDAW,JUCE_VERSION>"
    JUCE_PLUGINHOST_VST3=1
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0)

target_link_libraries(OpenDAW PRIVATE
    tracktion::tracktion_core
    tracktion::tracktion_engine
    tracktion::tracktion_graph
    juce::juce_audio_devices
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

if (MSVC)
    target_compile_options(OpenDAW PRIVATE /bigobj /MP)
endif()
