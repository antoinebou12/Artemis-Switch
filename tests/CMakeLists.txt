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

add_artemis_test(host_address_parse_test
    tests/host_address_parse_test.cpp
    app/src/features/host/HostAddressParse.cpp
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

add_artemis_test(host_keyboard_shortcuts_test
    tests/host_keyboard_shortcuts_test.cpp
    app/src/features/input/HostKeyboardShortcuts.cpp
    app/src/features/input)

add_artemis_test(pointer_settings_test
    tests/pointer_settings_test.cpp
    app/src/features/input/PointerSettings.cpp
    app/src/features/input)

add_artemis_test(quick_stream_menu_test
    tests/quick_stream_menu_test.cpp
    app/src/features/ui/QuickStreamMenu.cpp
    app/src/features/ui)

add_artemis_test(zoom_pan_state_test
    tests/zoom_pan_state_test.cpp
    app/src/features/video/ZoomPanState.cpp
    app/src/features/video)

add_executable(usable_mac_test
    "${REPO_ROOT}/tests/usable_mac_test.cpp"
)
target_include_directories(usable_mac_test PRIVATE
    "${REPO_ROOT}/app/src/utils")
target_compile_features(usable_mac_test PRIVATE cxx_std_20)
add_test(NAME usable_mac_test COMMAND usable_mac_test)

add_artemis_test(display_transform_test
    tests/display_transform_test.cpp
    app/src/features/video/DisplayTransform.cpp
    app/src/features/video)

add_artemis_test(apollo_host_options_test
    tests/apollo_host_options_test.cpp
    app/src/features/apollo/ApolloHostOptions.cpp
    app/src/features/apollo)

add_artemis_test(controller_diagnostics_test
    tests/controller_diagnostics_test.cpp
    app/src/features/input/ControllerDiagnostics.cpp
    app/src/features/input)

add_executable(renderer_presentation_policy_test
    "${REPO_ROOT}/tests/renderer_presentation_policy_test.cpp"
    "${REPO_ROOT}/app/src/features/video/RendererPresentationPolicy.cpp"
    "${REPO_ROOT}/app/src/features/video/DisplayTransform.cpp"
    "${REPO_ROOT}/app/src/video/VideoScale.cpp")
target_include_directories(renderer_presentation_policy_test PRIVATE
    "${REPO_ROOT}/app/src/features/video"
    "${REPO_ROOT}/app/src/video")
target_compile_features(renderer_presentation_policy_test PRIVATE cxx_std_20)
add_test(NAME renderer_presentation_policy_test
         COMMAND renderer_presentation_policy_test)

add_artemis_test(touch_control_profile_test
    tests/touch_control_profile_test.cpp
    app/src/features/input/TouchControlProfile.cpp
    app/src/features/input)

add_artemis_test(app_locale_preference_test
    tests/app_locale_preference_test.cpp
    app/src/features/i18n/AppLocalePreference.cpp
    app/src/features/i18n)

add_executable(korean_keyboard_layout_test
    "${REPO_ROOT}/tests/korean_keyboard_layout_test.cpp")
target_compile_features(korean_keyboard_layout_test PRIVATE cxx_std_20)
add_test(NAME korean_keyboard_layout_test COMMAND korean_keyboard_layout_test)

add_executable(display_coordinate_mapper_test
    "${REPO_ROOT}/tests/display_coordinate_mapper_test.cpp"
    "${REPO_ROOT}/app/src/features/video/DisplayCoordinateMapper.cpp"
    "${REPO_ROOT}/app/src/features/video/RendererPresentationPolicy.cpp"
    "${REPO_ROOT}/app/src/features/video/DisplayTransform.cpp"
    "${REPO_ROOT}/app/src/video/VideoScale.cpp")
target_include_directories(display_coordinate_mapper_test PRIVATE
    "${REPO_ROOT}/app/src/features/video"
    "${REPO_ROOT}/app/src/video")
target_compile_features(display_coordinate_mapper_test PRIVATE cxx_std_20)
add_test(NAME display_coordinate_mapper_test
         COMMAND display_coordinate_mapper_test)
