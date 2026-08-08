cmake_minimum_required(VERSION 3.20)
project(ArtemisSwitchUnitTests LANGUAGES CXX)

enable_testing()

add_executable(artemis_test_smoke smoke_test.cpp)
target_compile_features(artemis_test_smoke PRIVATE cxx_std_20)
add_test(NAME artemis_test_smoke COMMAND artemis_test_smoke)

add_executable(artemis_test_zoom_pan
    zoom_pan_state_test.cpp
    ../app/src/features/video/ZoomPanState.cpp)
target_compile_features(artemis_test_zoom_pan PRIVATE cxx_std_20)
add_test(NAME artemis_test_zoom_pan COMMAND artemis_test_zoom_pan)

# Keep portable unit tests independent from libnx/Borealis/deko3d so they can
# run on standard Linux CI.
