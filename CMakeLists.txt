cmake_minimum_required(VERSION 3.22)

# プロジェクト名を定義
set(PROJECT_TARGET_NAME "OSCSender") #プラグイン名
set(PLUGIN_CODE "OSCs") # 識別子(4文字)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(JUCE_PATH "/path/to/JUCE" CACHE PATH "Path to JUCE")
add_subdirectory(${JUCE_PATH} _juce)

# 定義した変数を使用
project(${PROJECT_TARGET_NAME} VERSION 0.0.1)

file(GLOB_RECURSE WEB_ASSETS CONFIGURE_DEPENDS "Resources/dist/*")

juce_add_binary_data(Assets
    HEADER_NAME "BinaryData.h"
    NAMESPACE BinaryData
    SOURCES ${WEB_ASSETS}
)

juce_add_plugin(${PROJECT_TARGET_NAME}
    COMPANY_NAME "Shinh"
    PLUGIN_MANUFACTURER_CODE "Shih"
    PLUGIN_CODE ${PLUGIN_CODE}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    FORMATS VST3 AU Standalone
    PRODUCT_NAME ${PROJECT_TARGET_NAME})

target_compile_definitions(${PROJECT_NAME} PUBLIC 
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_WEB_BROWSER=1
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_audio_formats
    juce::juce_gui_extra
    juce::juce_osc
    juce::juce_dsp
    Assets
)

file(GLOB_RECURSE CPP_ASSETS CONFIGURE_DEPENDS "Source/*.cpp")

target_sources(${PROJECT_NAME} PRIVATE
    ${CPP_ASSETS}
)