# Max externals that run the kalabash DSP cores inside Max for Live devices.
# Configure with -DMAX_SDK_BASE=<path to Cycling74/max-sdk-base>.
cmake_minimum_required(VERSION 3.19)
project(kalabash_externals)

set(KB_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(MI ${KB_ROOT}/third_party/eurorack)
if(NOT MAX_SDK_BASE)
  message(FATAL_ERROR "Set MAX_SDK_BASE to a checkout of https://github.com/Cycling74/max-sdk-base")
endif()
set(C74_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/externals)
set(STMLIB_SRC ${MI}/stmlib/dsp/units.cc ${MI}/stmlib/dsp/atan.cc ${MI}/stmlib/utils/random.cc)

file(GLOB_RECURSE PLAITS_DSP ${MI}/plaits/dsp/*.cc)
file(GLOB_RECURSE ELEMENTS_DSP ${MI}/elements/dsp/*.cc)
file(GLOB_RECURSE CLOUDS_DSP ${MI}/clouds/dsp/*.cc)
file(GLOB_RECURSE BEADS_DSP ${MI}/beads/dsp/*.cc)
set(KB_SRC_kb.knotwork_tilde ${KB_ROOT}/cores/PlaitsCore.cpp ${PLAITS_DSP} ${MI}/plaits/resources.cc)
set(KB_SRC_kb.alloy_tilde ${KB_ROOT}/cores/ElementsCore.cpp ${ELEMENTS_DSP} ${MI}/elements/resources.cc)
set(KB_SRC_kb.fogbank_tilde ${KB_ROOT}/cores/CloudsCore.cpp ${CLOUDS_DSP} ${MI}/clouds/resources.cc)
set(KB_SRC_kb.abacus_tilde ${KB_ROOT}/cores/BeadsCore.cpp ${BEADS_DSP} ${MI}/beads/resources.cc)

foreach(ext kb.knotwork_tilde kb.alloy_tilde kb.fogbank_tilde kb.abacus_tilde)
  add_subdirectory(${ext})
endforeach()
