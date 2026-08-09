cmake_minimum_required(VERSION 3.20)
project(ArtemisSwitchUnitTests LANGUAGES CXX)

enable_testing()

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

add_executable(artemis_test_smoke smoke_test.cpp)
target_compile_features(artemis_test_smoke PRIVATE cxx_std_20)
add_test(NAME artemis_test_smoke COMMAND artemis_test_smoke)

function(add_artemis_test target test_source feature_source include_dir)
    add_executable(${target}
        "${REPO_ROOT}/${test_source}"
        "${REPO_ROOT}/${feature_source}"
    )
    target_include_directories(${target} PRIVATE "${REPO_ROOT}/${include_dir}")
    target_compile_features(${target} PRIVATE cxx_std_20)
    add_test(NAME ${target} COMMAND ${target})
endfunction()

add_artemis_test(benchmark_tests
    tests/benchmark_tests.cpp
    app/src/benchmark/BenchmarkAccumulator.cpp
    app/src/benchmark)

add_executable(benchmark_export_tests
    "${REPO_ROOT}/tests/benchmark_export_tests.cpp"
    "${REPO_ROOT}/app/src/benchmark/SwitchRuntimeMetadata.cpp"
    "${REPO_ROOT}/app/src/benchmark/BenchmarkExport.cpp"
    "${REPO_ROOT}/app/src/benchmark/BenchmarkFileStore.cpp"
)
target_include_directories(benchmark_export_tests PRIVATE
    "${REPO_ROOT}/app/src/benchmark")
target_compile_features(benchmark_export_tests PRIVATE cxx_std_20)
add_test(NAME benchmark_export_tests COMMAND benchmark_export_tests)

add_executable(auto_tune_tests
    "${REPO_ROOT}/tests/auto_tune_tests.cpp"
    "${REPO_ROOT}/app/src/benchmark/AutoTune.cpp"
    "${REPO_ROOT}/app/src/benchmark/AutoTunePlan.cpp"
)
target_include_directories(auto_tune_tests PRIVATE
    "${REPO_ROOT}/app/src/benchmark")
target_compile_features(auto_tune_tests PRIVATE cxx_std_20)
add_test(NAME auto_tune_tests COMMAND auto_tune_tests)

add_artemis_test(stream_profile_tests
    tests/stream_profile_tests.cpp
    app/src/streaming/SwitchStreamProfile.cpp
    app/src/streaming)

add_artemis_test(video_scale_tests
    tests/video_scale_tests.cpp
    app/src/video/VideoScale.cpp
    app/src/video)

add_artemis_test(host_capability_tests
    tests/host_capability_tests.cpp
    app/src/host/HostCapabilities.cpp
    app/src/host)

add_artemis_test(performance_lite_test
    tests/performance_lite_test.cpp
    app/src/features/performance/PerformanceLite.cpp
    app/src/features/performance)

add_artemis_test(advanced_stream_options_test
    tests/advanced_stream_options_test.cpp
    app/src/features/stream/AdvancedStreamOptions.cpp
    app/src/features/stream)

add_artemis_test(switch_motion_policy_test
    tests/switch_motion_policy_test.cpp
    app/src/features/input/SwitchMotionPolicy.cpp
    app/src/features/input)

add_artemis_test(quick_stream_menu_test
    tests/quick_stream_menu_test.cpp
    app/src/features/ui/QuickStreamMenu.cpp
    app/src/features/ui)

add_artemis_test(zoom_pan_state_test
    tests/zoom_pan_state_test.cpp
    app/src/features/video/ZoomPanState.cpp
    app/src/features/video)
