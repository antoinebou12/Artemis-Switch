cmake_minimum_required(VERSION 3.20)
project(ArtemisSwitchIntegrationTests LANGUAGES CXX)

enable_testing()

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

function(add_artemis_test target test_source feature_source include_dir)
    add_executable(${target}
        "${REPO_ROOT}/${test_source}"
        "${REPO_ROOT}/${feature_source}"
    )
    target_include_directories(${target} PRIVATE "${REPO_ROOT}/${include_dir}")
    target_compile_features(${target} PRIVATE cxx_std_20)
    add_test(NAME ${target} COMMAND ${target})
endfunction()

add_executable(app_library_paging_test
    "${REPO_ROOT}/tests/app_library_paging_test.cpp")
target_include_directories(app_library_paging_test PRIVATE
    "${REPO_ROOT}/app/src/features/ui")
target_compile_features(app_library_paging_test PRIVATE cxx_std_20)
add_test(NAME app_library_paging_test COMMAND app_library_paging_test)

add_artemis_test(benchmark_tests
    tests/benchmark_tests.cpp
    app/src/benchmark/BenchmarkAccumulator.cpp
    app/src)

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

add_artemis_test(codec_capabilities_test
    tests/codec_capabilities_test.cpp
    app/src/features/host/CodecCapabilities.cpp
    app/src/features/host)

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

add_artemis_test(controller_topology_test
    tests/controller_topology_test.cpp
    app/src/features/input/ControllerTopology.cpp
    app/src/features/input)

add_artemis_test(controller_battery_test
    tests/controller_battery_test.cpp
    app/src/features/input/ControllerBattery.cpp
    app/src/features/input)

add_artemis_test(host_keyboard_shortcuts_test
    tests/host_keyboard_shortcuts_test.cpp
    app/src/features/input/HostKeyboardShortcuts.cpp
    app/src/features/input)
target_include_directories(host_keyboard_shortcuts_test PRIVATE
    "${REPO_ROOT}/app/src/utils")

add_executable(host_identity_probe_test
    "${REPO_ROOT}/tests/host_identity_probe_test.cpp"
    "${REPO_ROOT}/app/src/host/HostIdentityProbe.cpp"
    "${REPO_ROOT}/app/src/host/HostCapabilities.cpp"
    "${REPO_ROOT}/app/src/features/host/HostAddressParse.cpp"
)
target_include_directories(host_identity_probe_test PRIVATE
    "${REPO_ROOT}/app/src/host"
    "${REPO_ROOT}/app/src/features/host"
    "${REPO_ROOT}/app/src/crypto"
    "${REPO_ROOT}/app/src/libgamestream")
target_compile_features(host_identity_probe_test PRIVATE cxx_std_20)
add_test(NAME host_identity_probe_test COMMAND host_identity_probe_test)

add_artemis_test(quick_stream_menu_test
    tests/quick_stream_menu_test.cpp
    app/src/features/ui/QuickStreamMenu.cpp
    app/src/features/ui)

add_artemis_test(zoom_pan_state_test
    tests/zoom_pan_state_test.cpp
    app/src/features/video/ZoomPanState.cpp
    app/src/features/video)
