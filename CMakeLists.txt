cmake_minimum_required(VERSION 3.20)
project(rangemaster_dsp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(RANGEMASTER_BUILD_TESTS "Build automated test suite" ON)
option(RANGEMASTER_BUILD_TOOLS "Build CLI renderer and benchmarks" ON)
option(RANGEMASTER_BUILD_WASM "Build WebAssembly module via Emscripten" OFF)

set(RANGEMASTER_CORE_SOURCES
    src/dsp/components/TransistorProfiles.cpp
    src/dsp/components/EbersMoll.cpp
    src/dsp/components/GummelPoon.cpp
    src/dsp/solver/NewtonSolver.cpp
    src/dsp/circuit/CircuitMatrices.cpp
    src/dsp/circuit/RangemasterCircuit.cpp
    src/dsp/core/RangemasterEngine.cpp
)

add_library(rangemaster_core STATIC ${RANGEMASTER_CORE_SOURCES})
target_include_directories(rangemaster_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(rangemaster_core PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -fno-math-errno>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

if(RANGEMASTER_BUILD_TOOLS AND NOT RANGEMASTER_BUILD_WASM)
    add_executable(rangemaster-render
        tools/renderer/WavIo.cpp
        tools/renderer/main.cpp
    )
    target_link_libraries(rangemaster-render PRIVATE rangemaster_core)
endif()

if(RANGEMASTER_BUILD_TESTS AND NOT RANGEMASTER_BUILD_WASM)
    enable_testing()

    add_executable(test_components tests/unit/test_components.cpp)
    target_link_libraries(test_components PRIVATE rangemaster_core)
    add_test(NAME unit.components COMMAND test_components)

    add_executable(test_dc_and_basic tests/circuit/test_dc_and_basic.cpp)
    target_link_libraries(test_dc_and_basic PRIVATE rangemaster_core)
    add_test(NAME circuit.dc_and_basic COMMAND test_dc_and_basic)

    add_executable(test_ac_ebers_moll tests/circuit/test_ac_ebers_moll.cpp)
    target_link_libraries(test_ac_ebers_moll PRIVATE rangemaster_core)
    add_test(NAME circuit.ac_ebers_moll COMMAND test_ac_ebers_moll)

    add_executable(test_gummel_poon tests/unit/test_gummel_poon.cpp)
    target_link_libraries(test_gummel_poon PRIVATE rangemaster_core)
    add_test(NAME unit.gummel_poon COMMAND test_gummel_poon)

    add_executable(test_all_engines_profiles tests/circuit/test_all_engines_profiles.cpp)
    target_link_libraries(test_all_engines_profiles PRIVATE rangemaster_core)
    add_test(NAME circuit.all_engines_profiles COMMAND test_all_engines_profiles)
endif()
