# =============================================================================
#  CUPIDO — Suíte vocal com IA
#  Build:  cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --config Release
#  Formatos: VST3 (Win/Mac/Linux), AU (Mac), LV2 (Linux), Standalone, AAX (se AAX_SDK_PATH)
# =============================================================================
cmake_minimum_required(VERSION 3.22)
project(CUPIDO VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "macOS mínimo")
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary" FORCE)
endif()

# ---- JUCE (baixado automaticamente; ou use -DCUPIDO_JUCE_DIR=/caminho/JUCE) ----
include(FetchContent)
set(CUPIDO_JUCE_DIR "" CACHE PATH "Pasta local do JUCE (opcional)")
if(CUPIDO_JUCE_DIR)
    add_subdirectory(${CUPIDO_JUCE_DIR} juce EXCLUDE_FROM_ALL)
else()
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.12
        GIT_SHALLOW    ON)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---- Formatos ----
set(CUPIDO_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND CUPIDO_FORMATS AU)
endif()
if(UNIX AND NOT APPLE)
    list(APPEND CUPIDO_FORMATS LV2)
endif()
set(AAX_SDK_PATH "" CACHE PATH "Pasta do AAX SDK da Avid (para Pro Tools)")
if(AAX_SDK_PATH)
    juce_set_aax_sdk_path(${AAX_SDK_PATH})
    list(APPEND CUPIDO_FORMATS AAX)
endif()

juce_add_plugin(Cupido
    PRODUCT_NAME              "CUPIDO"
    COMPANY_NAME              "Cupido Audio"
    COMPANY_WEBSITE           "https://cupido.audio"
    COMPANY_EMAIL             "cupidostore01@gmail.com"
    BUNDLE_ID                 "com.cupidoaudio.cupido"
    PLUGIN_MANUFACTURER_CODE  Cpdo
    PLUGIN_CODE               Cpd1
    DESCRIPTION               "Suite vocal com IA: autotune, mix e master"
    FORMATS                   ${CUPIDO_FORMATS}
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD   FALSE
    NEEDS_WEB_BROWSER         TRUE
    NEEDS_WEBVIEW2            TRUE
    NEEDS_CURL                TRUE
    MICROPHONE_PERMISSION_ENABLED TRUE
    HARDENED_RUNTIME_ENABLED  TRUE
    HARDENED_RUNTIME_OPTIONS  com.apple.security.device.audio-input com.apple.security.network.client
    MICROPHONE_PERMISSION_TEXT "O CUPIDO usa o microfone para comandos de voz (segure o botão do Cupidinho e fale)."
    VST3_CATEGORIES           Fx Dynamics EQ Reverb Delay
    AU_MAIN_TYPE              kAudioUnitType_Effect
    AAX_CATEGORY              Dynamics EQ PitchShift
    LV2URI                    "https://cupido.audio/plugins/cupido"
    ICON_BIG                  "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.png")

# ---- Interface (HTML/CSS/JS) embutida no binário ----
file(GLOB CUPIDO_UI_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/ui/*")
juce_add_binary_data(CupidoUI NAMESPACE CupidoUI HEADER_NAME BinaryData.h SOURCES ${CUPIDO_UI_FILES})

target_sources(Cupido PRIVATE
    source/PluginProcessor.cpp
    source/PluginEditor.cpp
    source/AiClient.cpp)

target_include_directories(Cupido PRIVATE source)

target_compile_definitions(Cupido PUBLIC
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

target_link_libraries(Cupido
    PRIVATE
        CupidoUI
        juce::juce_audio_utils
        juce::juce_audio_devices
        juce::juce_gui_extra
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---- Testes do motor de áudio (não precisam do JUCE) ----
option(CUPIDO_BUILD_TESTS "Compila os testes do DSP" ON)
if(CUPIDO_BUILD_TESTS)
    add_executable(cupido_dsp_tests tools/test_dsp.cpp)
    add_executable(cupido_gen_params tools/gen_params.cpp)
endif()
