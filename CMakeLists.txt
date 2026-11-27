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
    src/dsp/oversampling/HalfbandFilter.cpp
    src/dsp/oversampling/Oversampler.cpp
    src/dsp/core/RangemasterEngine.cpp
)

add_library(rangemaster_core STATIC ${RANGEMASTER_CORE_SOURCES})
target_include_directories(rangemaster_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(rangemaster_core PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -fno-math-errno>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

add_library(rangemaster_c_api STATIC src/abi/rangemaster_c_api.cpp)
target_link_libraries(rangemaster_c_api PUBLIC rangemaster_core)
target_compile_options(rangemaster_c_api PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -fno-math-errno>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

if(RANGEMASTER_BUILD_WASM)
    add_executable(rangemaster_dsp src/abi/rangemaster_c_api.cpp)
    target_link_libraries(rangemaster_dsp PRIVATE rangemaster_core)
    set_target_properties(rangemaster_dsp PROPERTIES
        OUTPUT_NAME "rangemaster_dsp"
        SUFFIX ".js"
    )
    target_link_options(rangemaster_dsp PRIVATE
        "-sMODULARIZE=1"
        "-sEXPORT_ES6=1"
        "-sENVIRONMENT=web,worker,node"
        "-sINITIAL_MEMORY=16777216"
        "-sALLOW_MEMORY_GROWTH=0"
        "-sEXPORTED_RUNTIME_METHODS=[\"HEAPF32\",\"HEAPF64\",\"cwrap\",\"ccall\"]"
        "-sEXPORTED_FUNCTIONS=[\"_malloc\",\"_free\",\"_rangemaster_apply_config\",\"_rangemaster_get_config\",\"_rangemaster_get_config_abi_version\",\"_rangemaster_get_config_struct_size\",\"_rangemaster_create\",\"_rangemaster_destroy\",\"_rangemaster_prepare\",\"_rangemaster_reset\",\"_rangemaster_reset_diagnostics\",\"_rangemaster_process_f64\",\"_rangemaster_process_f32\",\"_rangemaster_set_engine\",\"_rangemaster_set_transistor\",\"_rangemaster_set_transistor_by_name\",\"_rangemaster_set_oversampling\",\"_rangemaster_set_boost\",\"_rangemaster_set_pot_taper\",\"_rangemaster_set_boost_linear_approx\",\"_rangemaster_set_input_volts\",\"_rangemaster_set_output_ref_volts\",\"_rangemaster_set_output_trim_db\",\"_rangemaster_set_range_nf\",\"_rangemaster_set_source_ohms\",\"_rangemaster_set_load_ohms\",\"_rangemaster_set_supply_volts\",\"_rangemaster_set_bias_trim\",\"_rangemaster_set_solver_profile\",\"_rangemaster_set_gp_ablation\",\"_rangemaster_get_sample_rate\",\"_rangemaster_get_internal_sample_rate\",\"_rangemaster_get_latency_samples\",\"_rangemaster_get_matrix_rebuild_count\",\"_rangemaster_get_conditioning\",\"_rangemaster_get_diagnostics\",\"_rangemaster_get_dc_operating_point\"]"
    )
endif()

if(RANGEMASTER_BUILD_TOOLS AND NOT RANGEMASTER_BUILD_WASM)
    add_executable(rangemaster-render
        tools/renderer/WavIo.cpp
        tools/renderer/main.cpp
    )
    target_link_libraries(rangemaster-render PRIVATE rangemaster_core)

    add_executable(rangemaster-bench
        tools/benchmarks/main.cpp
    )
    target_link_libraries(rangemaster-bench PRIVATE rangemaster_core)
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

    add_executable(test_robustness tests/numerical/test_robustness.cpp)
    target_link_libraries(test_robustness PRIVATE rangemaster_core)
    add_test(NAME numerical.robustness COMMAND test_robustness)

    add_executable(test_consistency tests/regression/test_consistency.cpp)
    target_link_libraries(test_consistency PRIVATE rangemaster_core)
    add_test(NAME regression.consistency COMMAND test_consistency)

    add_executable(test_c_api tests/unit/test_c_api.cpp)
    target_link_libraries(test_c_api PRIVATE rangemaster_c_api)
    add_test(NAME unit.c_api COMMAND test_c_api)

    add_executable(test_m7_hardening tests/unit/test_m7_hardening.cpp)
    target_link_libraries(test_m7_hardening PRIVATE rangemaster_c_api)
    add_test(NAME unit.m7_hardening COMMAND test_m7_hardening)
endif()
