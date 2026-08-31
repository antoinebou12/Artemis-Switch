cmake_minimum_required(VERSION 3.20)
project(ArtemisSwitchUnitTests LANGUAGES C CXX)

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

option(ARTEMIS_STRICT_WARNINGS
    "Treat warnings in the portable Artemis test surface as errors" OFF)
option(ARTEMIS_ENABLE_CLANG_TIDY
    "Run clang-tidy while compiling the portable Artemis test surface" OFF)

if(ARTEMIS_STRICT_WARNINGS)
    if(MSVC)
        add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/W4>
                            $<$<COMPILE_LANGUAGE:CXX>:/WX>)
        # Portable code intentionally uses standard C APIs such as getenv;
        # MSVC's non-standard secure-CRT deprecations are not actionable here.
        add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    else()
        add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wall>
                            $<$<COMPILE_LANGUAGE:CXX>:-Wextra>
                            $<$<COMPILE_LANGUAGE:CXX>:-Wpedantic>
                            $<$<COMPILE_LANGUAGE:CXX>:-Werror>)
    endif()
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # Borealis currently vendors an older nlohmann/json header whose
        # literal-operator spelling is deprecated by recent Clang versions.
        add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-literal-operator>)
    endif()
endif()

if(ARTEMIS_ENABLE_CLANG_TIDY)
    find_program(ARTEMIS_CLANG_TIDY_EXECUTABLE NAMES clang-tidy REQUIRED)
    set(CMAKE_CXX_CLANG_TIDY
        "${ARTEMIS_CLANG_TIDY_EXECUTABLE};--config-file=${REPO_ROOT}/.clang-tidy")
    if(MSVC)
        # clang-tidy does not consistently preserve clang-cl's /EHsc through
        # CMake's compiler launcher, so make the parse mode explicit.
        list(APPEND CMAKE_CXX_CLANG_TIDY "--extra-arg=/EHsc")
    endif()
endif()

enable_testing()

add_executable(artemis_test_smoke smoke_test.cpp)
target_compile_features(artemis_test_smoke PRIVATE cxx_std_20)
add_test(NAME artemis_test_smoke COMMAND artemis_test_smoke)

add_executable(cancellation_token_test cancellation_token_test.cpp)
target_compile_features(cancellation_token_test PRIVATE cxx_std_20)
add_test(NAME cancellation_token_test COMMAND cancellation_token_test)

add_executable(app_library_paging_test app_library_paging_test.cpp)
target_include_directories(app_library_paging_test PRIVATE
    "${REPO_ROOT}/app/src/features/ui")
target_compile_features(app_library_paging_test PRIVATE cxx_std_20)
add_test(NAME app_library_paging_test COMMAND app_library_paging_test)

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

add_executable(stream_config_profile_tests
    "${REPO_ROOT}/tests/stream_config_profile_tests.cpp"
)
target_include_directories(stream_config_profile_tests PRIVATE
    "${REPO_ROOT}/app/src/streaming")
target_compile_features(stream_config_profile_tests PRIVATE cxx_std_20)
add_test(NAME stream_config_profile_tests COMMAND stream_config_profile_tests)

add_executable(stream_disconnect_policy_tests
    "${REPO_ROOT}/tests/stream_disconnect_policy_tests.cpp"
)
target_include_directories(stream_disconnect_policy_tests PRIVATE
    "${REPO_ROOT}/app/src/streaming")
target_compile_features(stream_disconnect_policy_tests PRIVATE cxx_std_20)
add_test(NAME stream_disconnect_policy_tests COMMAND stream_disconnect_policy_tests)

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

add_executable(present_deadline_test
    "${REPO_ROOT}/tests/present_deadline_test.cpp"
)
target_include_directories(present_deadline_test PRIVATE
    "${REPO_ROOT}/app/src/streaming")
target_compile_features(present_deadline_test PRIVATE cxx_std_20)
add_test(NAME present_deadline_test COMMAND present_deadline_test)

add_executable(frame_color_range_test
    "${REPO_ROOT}/tests/frame_color_range_test.cpp"
)
target_include_directories(frame_color_range_test PRIVATE
    "${REPO_ROOT}/app/src/streaming")
target_compile_features(frame_color_range_test PRIVATE cxx_std_20)
add_test(NAME frame_color_range_test COMMAND frame_color_range_test)

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

add_artemis_test(frame_rate_options_test
    tests/frame_rate_options_test.cpp
    app/src/features/stream/FrameRateOptions.cpp
    app/src/features/stream)

add_artemis_test(video_codec_formats_test
    tests/video_codec_formats_test.cpp
    app/src/features/stream/VideoCodecFormats.cpp
    app/src/features/stream)

add_artemis_test(bitrate_steps_test
    tests/bitrate_steps_test.cpp
    app/src/features/stream/BitrateSteps.cpp
    app/src/features/stream)

add_executable(stick_deadzone_test
    "${REPO_ROOT}/tests/stick_deadzone_test.cpp")
target_include_directories(stick_deadzone_test PRIVATE
    "${REPO_ROOT}/app/src/features/input")
target_compile_features(stick_deadzone_test PRIVATE cxx_std_20)
add_test(NAME stick_deadzone_test COMMAND stick_deadzone_test)

add_artemis_test(switch_motion_policy_test
    tests/switch_motion_policy_test.cpp
    app/src/features/input/SwitchMotionPolicy.cpp
    app/src/features/input)

add_artemis_test(controller_topology_test
    tests/controller_topology_test.cpp
    app/src/features/input/ControllerTopology.cpp
    app/src/features/input)

add_artemis_test(controller_session_reset_test
    tests/controller_session_reset_test.cpp
    app/src/features/input/ControllerSessionReset.cpp
    app/src/features/input)

add_artemis_test(controller_battery_test
    tests/controller_battery_test.cpp
    app/src/features/input/ControllerBattery.cpp
    app/src/features/input)

add_executable(host_keyboard_shortcuts_test
    "${REPO_ROOT}/tests/host_keyboard_shortcuts_test.cpp"
    "${REPO_ROOT}/app/src/features/input/HostKeyboardShortcuts.cpp")
target_include_directories(host_keyboard_shortcuts_test PRIVATE
    "${REPO_ROOT}/app/src/features/input"
    "${REPO_ROOT}/app/src/utils")
target_compile_features(host_keyboard_shortcuts_test PRIVATE cxx_std_20)
add_test(NAME host_keyboard_shortcuts_test COMMAND host_keyboard_shortcuts_test)

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

add_artemis_test(server_command_shortcuts_test
    tests/server_command_shortcuts_test.cpp
    app/src/features/apollo/ServerCommandShortcuts.cpp
    app/src/features/apollo)

add_artemis_test(wireguard_config_test
    tests/wireguard_config_test.cpp
    app/src/vpn/WireGuardConfig.cpp
    app/src/vpn)

add_artemis_test(remote_access_isolation_test
    tests/remote_access_isolation_test.cpp
    app/src/remote_access/RemoteAccessManager.cpp
    app/src/remote_access)

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

add_artemis_test(stats_overlay_layout_test
    tests/stats_overlay_layout_test.cpp
    app/src/features/ui/StatsOverlayLayout.cpp
    app/src/features/ui)

add_executable(upscaling_mode_select_test
    "${REPO_ROOT}/tests/upscaling_mode_select_test.cpp")
target_include_directories(upscaling_mode_select_test PRIVATE
    "${REPO_ROOT}/app/src/features/video")
target_compile_features(upscaling_mode_select_test PRIVATE cxx_std_20)
add_test(NAME upscaling_mode_select_test COMMAND upscaling_mode_select_test)

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

add_executable(host_endpoints_test
    "${REPO_ROOT}/tests/host_endpoints_test.cpp"
)
target_compile_features(host_endpoints_test PRIVATE cxx_std_20)
add_test(NAME host_endpoints_test COMMAND host_endpoints_test)

add_executable(remote_access_route_lifecycle_test
    "${REPO_ROOT}/tests/remote_access_route_lifecycle_test.cpp"
    "${REPO_ROOT}/app/src/remote_access/RemoteAccessManager.cpp"
)
target_include_directories(remote_access_route_lifecycle_test PRIVATE
    "${REPO_ROOT}/app/src/remote_access")
target_compile_features(remote_access_route_lifecycle_test PRIVATE cxx_std_20)
add_test(NAME remote_access_route_lifecycle_test
         COMMAND remote_access_route_lifecycle_test)

add_executable(remote_access_manager_concurrency_test
    "${REPO_ROOT}/tests/remote_access_manager_concurrency_test.cpp"
    "${REPO_ROOT}/app/src/remote_access/RemoteAccessManager.cpp"
)
target_include_directories(remote_access_manager_concurrency_test PRIVATE
    "${REPO_ROOT}/app/src/remote_access")
target_compile_features(remote_access_manager_concurrency_test PRIVATE cxx_std_20)
add_test(NAME remote_access_manager_concurrency_test
         COMMAND remote_access_manager_concurrency_test)

add_executable(tailscale_protocol_test
    "${REPO_ROOT}/tests/tailscale_protocol_test.cpp"
    "${REPO_ROOT}/app/src/remote_access/tailscale/TailscaleProtocol.cpp")
target_include_directories(tailscale_protocol_test PRIVATE
    "${REPO_ROOT}/app/src/remote_access/tailscale")
target_compile_features(tailscale_protocol_test PRIVATE cxx_std_20)
add_test(NAME tailscale_protocol_test COMMAND tailscale_protocol_test)

add_executable(tailscale_auth_key_file_test
    "${REPO_ROOT}/tests/tailscale_auth_key_file_test.cpp"
    "${REPO_ROOT}/app/src/remote_access/tailscale/TailscaleAuthKeyFile.cpp")
target_include_directories(tailscale_auth_key_file_test PRIVATE
    "${REPO_ROOT}/app/src/remote_access/tailscale")
target_compile_features(tailscale_auth_key_file_test PRIVATE cxx_std_20)
add_test(NAME tailscale_auth_key_file_test COMMAND tailscale_auth_key_file_test)

add_executable(tailscale_peer_path_test
    "${REPO_ROOT}/tests/tailscale_peer_path_test.cpp"
    "${REPO_ROOT}/app/src/remote_access/tailscale/TailscalePeerDirectory.cpp"
    "${REPO_ROOT}/app/src/remote_access/tailscale/TailscalePathManager.cpp")
target_include_directories(tailscale_peer_path_test PRIVATE
    "${REPO_ROOT}/app/src/remote_access"
    "${REPO_ROOT}/app/src/remote_access/tailscale")
target_compile_features(tailscale_peer_path_test PRIVATE cxx_std_20)
add_test(NAME tailscale_peer_path_test COMMAND tailscale_peer_path_test)

add_executable(tailscale_state_store_test
    "${REPO_ROOT}/tests/tailscale_state_store_test.cpp"
    "${REPO_ROOT}/app/src/remote_access/tailscale/TailscaleStateStore.cpp"
    "${REPO_ROOT}/extern/wg-nx/library/crypto/monocypher.c")
target_include_directories(tailscale_state_store_test PRIVATE
    "${REPO_ROOT}/app/src/remote_access/tailscale"
    "${REPO_ROOT}/extern/wg-nx/library/crypto")
target_compile_features(tailscale_state_store_test PRIVATE cxx_std_20)
add_test(NAME tailscale_state_store_test COMMAND tailscale_state_store_test)

include("${REPO_ROOT}/cmake/TailscaleCompatibility.cmake")
artemis_generate_tailscale_compatibility(
    "${CMAKE_CURRENT_BINARY_DIR}/generated/tailscale")

add_executable(tailscale_control_codec_test
    "${REPO_ROOT}/tests/tailscale_control_codec_test.cpp"
    "${REPO_ROOT}/app/src/remote_access/tailscale/TailscaleControlCodec.cpp"
    "${REPO_ROOT}/app/src/remote_access/tailscale/TailscalePeerDirectory.cpp")
target_include_directories(tailscale_control_codec_test PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/generated/tailscale"
    "${REPO_ROOT}/app/src/remote_access"
    "${REPO_ROOT}/app/src/remote_access/tailscale"
    "${REPO_ROOT}/extern/borealis/library/include")
target_compile_features(tailscale_control_codec_test PRIVATE cxx_std_20)
add_test(NAME tailscale_control_codec_test COMMAND tailscale_control_codec_test)

add_executable(tailscale_control_key_test
    "${REPO_ROOT}/tests/tailscale_control_key_test.cpp"
    "${REPO_ROOT}/app/src/remote_access/tailscale/TailscaleControlKey.cpp"
    "${REPO_ROOT}/app/src/remote_access/tailscale/TailscaleControlCodec.cpp"
    "${REPO_ROOT}/app/src/remote_access/tailscale/TailscalePeerDirectory.cpp")
target_include_directories(tailscale_control_key_test PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/generated/tailscale"
    "${REPO_ROOT}/app/src/remote_access"
    "${REPO_ROOT}/app/src/remote_access/tailscale"
    "${REPO_ROOT}/extern/borealis/library/include")
target_compile_features(tailscale_control_key_test PRIVATE cxx_std_20)
add_test(NAME tailscale_control_key_test COMMAND tailscale_control_key_test)

add_executable(tailscale_control_session_test
    "${REPO_ROOT}/tests/tailscale_control_session_test.cpp"
    "${REPO_ROOT}/app/src/remote_access/tailscale/TailscaleControlSession.cpp"
    "${REPO_ROOT}/app/src/remote_access/tailscale/TailscaleControlCodec.cpp"
    "${REPO_ROOT}/app/src/remote_access/tailscale/TailscaleHttpUpgrade.cpp"
    "${REPO_ROOT}/app/src/remote_access/tailscale/TailscaleNoise.cpp"
    "${REPO_ROOT}/app/src/remote_access/tailscale/TailscaleStateStore.cpp"
    "${REPO_ROOT}/app/src/remote_access/tailscale/TailscalePeerDirectory.cpp"
    "${REPO_ROOT}/app/src/remote_access/tailscale/TailscaleTransport.cpp"
    "${REPO_ROOT}/extern/wg-nx/library/crypto/monocypher.c"
    "${REPO_ROOT}/extern/wg-nx/library/crypto/blake2s.c"
    "${REPO_ROOT}/extern/wg-nx/src/blake2s_neon.c")
target_include_directories(tailscale_control_session_test PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/generated/tailscale"
    "${REPO_ROOT}/app/src/remote_access"
    "${REPO_ROOT}/app/src/remote_access/tailscale"
    "${REPO_ROOT}/extern/wg-nx/library/crypto"
    "${REPO_ROOT}/extern/wg-nx/src"
    "${REPO_ROOT}/extern/borealis/library/include")
target_compile_features(tailscale_control_session_test PRIVATE cxx_std_20)
if(WIN32)
    target_link_libraries(tailscale_control_session_test PRIVATE ws2_32)
endif()
add_test(NAME tailscale_control_session_test COMMAND tailscale_control_session_test)

add_executable(tailscale_noise_test
    "${REPO_ROOT}/tests/tailscale_noise_test.cpp"
    "${REPO_ROOT}/app/src/remote_access/tailscale/TailscaleNoise.cpp"
    "${REPO_ROOT}/extern/wg-nx/library/crypto/monocypher.c"
    "${REPO_ROOT}/extern/wg-nx/library/crypto/blake2s.c"
    "${REPO_ROOT}/extern/wg-nx/src/blake2s_neon.c")
target_include_directories(tailscale_noise_test PRIVATE
    "${REPO_ROOT}/app/src/remote_access"
    "${REPO_ROOT}/app/src/remote_access/tailscale"
    "${REPO_ROOT}/extern/wg-nx/library/crypto"
    "${REPO_ROOT}/extern/wg-nx/src")
target_compile_features(tailscale_noise_test PRIVATE cxx_std_20)
add_test(NAME tailscale_noise_test COMMAND tailscale_noise_test)

add_executable(tailscale_http_upgrade_test
    "${REPO_ROOT}/tests/tailscale_http_upgrade_test.cpp"
    "${REPO_ROOT}/app/src/remote_access/tailscale/TailscaleHttpUpgrade.cpp")
target_include_directories(tailscale_http_upgrade_test PRIVATE
    "${REPO_ROOT}/app/src/remote_access/tailscale")
target_compile_features(tailscale_http_upgrade_test PRIVATE cxx_std_20)
add_test(NAME tailscale_http_upgrade_test COMMAND tailscale_http_upgrade_test)

add_executable(tailscale_core_engine_test
    "${REPO_ROOT}/tests/tailscale_core_engine_test.cpp"
    "${REPO_ROOT}/app/src/remote_access/tailscale/TailscaleCore.cpp"
    "${REPO_ROOT}/app/src/remote_access/tailscale/TailscaleStateStore.cpp"
    "${REPO_ROOT}/app/src/remote_access/tailscale/TailscalePeerDirectory.cpp"
    "${REPO_ROOT}/app/src/remote_access/tailscale/TailscalePathManager.cpp"
    "${REPO_ROOT}/extern/wg-nx/library/crypto/monocypher.c")
target_include_directories(tailscale_core_engine_test PRIVATE
    "${REPO_ROOT}/app/src/remote_access"
    "${REPO_ROOT}/app/src/remote_access/tailscale"
    "${REPO_ROOT}/extern/wg-nx/library/crypto")
target_compile_features(tailscale_core_engine_test PRIVATE cxx_std_20)
add_test(NAME tailscale_core_engine_test COMMAND tailscale_core_engine_test)

add_executable(tailscale_derp_test
    "${REPO_ROOT}/tests/tailscale_derp_test.cpp"
    "${REPO_ROOT}/app/src/remote_access/tailscale/TailscaleDerp.cpp")
target_include_directories(tailscale_derp_test PRIVATE
    "${REPO_ROOT}/app/src/remote_access/tailscale")
target_compile_features(tailscale_derp_test PRIVATE cxx_std_20)
add_test(NAME tailscale_derp_test COMMAND tailscale_derp_test)

add_executable(tailscale_wgx_route_test
    "${REPO_ROOT}/tests/tailscale_wgx_route_test.cpp"
    "${REPO_ROOT}/app/src/remote_access/tailscale/TailscaleWgxRoute.cpp")
target_include_directories(tailscale_wgx_route_test PRIVATE
    "${REPO_ROOT}/app/src/remote_access"
    "${REPO_ROOT}/app/src/remote_access/tailscale")
target_compile_features(tailscale_wgx_route_test PRIVATE cxx_std_20)
add_test(NAME tailscale_wgx_route_test COMMAND tailscale_wgx_route_test)

add_executable(remote_access_provider_selection_test
    "${REPO_ROOT}/tests/remote_access_provider_selection_test.cpp")
target_include_directories(remote_access_provider_selection_test PRIVATE
    "${REPO_ROOT}/app/include")
target_compile_features(remote_access_provider_selection_test PRIVATE cxx_std_20)
add_test(NAME remote_access_provider_selection_test
         COMMAND remote_access_provider_selection_test)

add_executable(vpn_file_logger_test
    "${REPO_ROOT}/tests/vpn_file_logger_test.cpp"
    "${REPO_ROOT}/app/src/vpn/VpnFileLogger.cpp")
target_include_directories(vpn_file_logger_test PRIVATE
    "${REPO_ROOT}/app/src/vpn")
target_compile_features(vpn_file_logger_test PRIVATE cxx_std_20)
add_test(NAME vpn_file_logger_test COMMAND vpn_file_logger_test)
