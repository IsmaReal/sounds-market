cmake_minimum_required(VERSION 3.22)

project(Flow VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(FLOW_BUILD_TESTS "Compilar los tests de la envolvente" ON)

# Copia el plugin a la carpeta de plugins del sistema después de cada compilación
# (en Windows: C:\Program Files\Common Files\VST3). El CI lo desactiva.
option(FLOW_COPY_AFTER_BUILD "Instalar el plugin en la carpeta del sistema al compilar" ON)

# JUCE: si ya lo tenés clonado, pasá -DJUCE_DIR=/ruta/a/JUCE; si no, se descarga solo.
set(JUCE_DIR "" CACHE PATH "Ruta a una copia local de JUCE (opcional)")

if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(FLOW_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND FLOW_FORMATS AU)
endif()

juce_add_plugin(Flow
    COMPANY_NAME "Sounds Market"
    PRODUCT_NAME "Flow"
    PLUGIN_MANUFACTURER_CODE SdMk
    PLUGIN_CODE Flw1
    BUNDLE_ID com.soundsmarket.flow
    FORMATS ${FLOW_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${FLOW_COPY_AFTER_BUILD}
    VST3_CATEGORIES Fx Dynamics Reverb)

set(FLOW_SOURCES
    Source/Modules.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/EnvelopeEditor.cpp
    Source/FlowLookAndFeel.cpp
    Source/RackPanel.cpp
    Source/ModuleControls.cpp)

target_sources(Flow PRIVATE ${FLOW_SOURCES})

target_compile_definitions(Flow PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Flow
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(FLOW_BUILD_TESTS)
    enable_testing()
    add_executable(EnvelopeTests Tests/EnvelopeTests.cpp)
    target_include_directories(EnvelopeTests PRIVATE Source)
    target_compile_features(EnvelopeTests PRIVATE cxx_std_17)
    add_test(NAME EnvelopeTests COMMAND EnvelopeTests)

    # Simula un host reproduciendo y verifica el gain que aplica el procesador.
    juce_add_console_app(ProcessorTests PRODUCT_NAME "ProcessorTests")
    target_sources(ProcessorTests PRIVATE Tests/ProcessorTests.cpp ${FLOW_SOURCES})
    target_include_directories(ProcessorTests PRIVATE Source)
    target_compile_definitions(ProcessorTests PRIVATE
        JucePlugin_Name="Flow"
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)
    target_link_libraries(ProcessorTests PRIVATE
        juce::juce_audio_utils
        juce::juce_recommended_config_flags)
    add_test(NAME ProcessorTests COMMAND ProcessorTests)
endif()
