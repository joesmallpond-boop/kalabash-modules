cmake_minimum_required(VERSION 3.22)
project(KALABASH_MODULES VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

option(KB_FORMATS "Plugin formats" "")
if(NOT KB_FORMATS)
  if(APPLE)
    set(KB_FORMATS VST3 AU)
  else()
    set(KB_FORMATS VST3)
  endif()
endif()

set(JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout (optional)")
if(JUCE_DIR)
  add_subdirectory(${JUCE_DIR} juce)
else()
  include(FetchContent)
  FetchContent_Declare(juce GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(juce)
endif()

set(MI ${CMAKE_SOURCE_DIR}/third_party/eurorack)
set(STMLIB_SRC ${MI}/stmlib/dsp/units.cc ${MI}/stmlib/dsp/atan.cc ${MI}/stmlib/utils/random.cc)

function(kb_add_plugin TARGET NAME CODE IS_SYNTH)
  set(options)
  set(multi SOURCES MI_SOURCES)
  cmake_parse_arguments(A "" "" "${multi}" ${ARGN})
  juce_add_plugin(${TARGET}
    PRODUCT_NAME "${NAME}"
    COMPANY_NAME "kalabash"
    COMPANY_WEBSITE "https://github.com/joesmallpond-boop"
    BUNDLE_ID "com.kalabash.${TARGET}"
    PLUGIN_MANUFACTURER_CODE Klbs
    PLUGIN_CODE ${CODE}
    FORMATS ${KB_FORMATS}
    IS_SYNTH ${IS_SYNTH}
    NEEDS_MIDI_INPUT ${IS_SYNTH}
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES $<IF:$<BOOL:${IS_SYNTH}>,Instrument,Fx>
    AU_MAIN_TYPE $<IF:$<BOOL:${IS_SYNTH}>,kAudioUnitType_MusicDevice,kAudioUnitType_MusicEffect>)
  target_sources(${TARGET} PRIVATE ${A_SOURCES} ${A_MI_SOURCES} ${STMLIB_SRC}
    ${CMAKE_SOURCE_DIR}/common/PanelUI.cpp)
  # The original firmware sources are built in their desktop ("TEST") configuration.
  set_source_files_properties(${A_MI_SOURCES} ${STMLIB_SRC} PROPERTIES
    COMPILE_OPTIONS "-include;cstdio;-w" COMPILE_DEFINITIONS TEST=1)
  set_source_files_properties(${A_SOURCES} PROPERTIES COMPILE_DEFINITIONS TEST=1)
  target_include_directories(${TARGET} PRIVATE ${MI} ${CMAKE_SOURCE_DIR})
  target_compile_definitions(${TARGET} PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
  target_link_libraries(${TARGET} PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
endfunction()

file(GLOB_RECURSE PLAITS_DSP ${MI}/plaits/dsp/*.cc)
kb_add_plugin(knotwork "KNOTWORK" Knot TRUE
  SOURCES plugins/knotwork/Knotwork.cpp cores/PlaitsCore.cpp
  MI_SOURCES ${PLAITS_DSP} ${MI}/plaits/resources.cc)

file(GLOB_RECURSE ELEMENTS_DSP ${MI}/elements/dsp/*.cc)
kb_add_plugin(alloy "ALLOY" Allo TRUE
  SOURCES plugins/alloy/Alloy.cpp cores/ElementsCore.cpp
  MI_SOURCES ${ELEMENTS_DSP} ${MI}/elements/resources.cc)

file(GLOB_RECURSE CLOUDS_DSP ${MI}/clouds/dsp/*.cc)
kb_add_plugin(fogbank "FOGBANK" Fogb FALSE
  SOURCES plugins/fogbank/Fogbank.cpp cores/CloudsCore.cpp
  MI_SOURCES ${CLOUDS_DSP} ${MI}/clouds/resources.cc)

file(GLOB_RECURSE BEADS_DSP ${MI}/beads/dsp/*.cc)
kb_add_plugin(abacus "ABACUS" Abac FALSE
  SOURCES plugins/abacus/Abacus.cpp cores/BeadsCore.cpp
  MI_SOURCES ${BEADS_DSP} ${MI}/beads/resources.cc)
