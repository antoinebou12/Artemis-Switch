cmake_minimum_required(VERSION 3.20)
project(ArtemisSwitchUnitTests LANGUAGES CXX)

enable_testing()

add_executable(artemis_test_smoke smoke_test.cpp)
target_compile_features(artemis_test_smoke PRIVATE cxx_std_20)
add_test(NAME artemis_test_smoke COMMAND artemis_test_smoke)

add_executable(artemis_test_switch_motion_policy
    switch_motion_policy_test.cpp
    ../app/src/features/input/SwitchMotionPolicy.cpp)
target_compile_features(artemis_test_switch_motion_policy PRIVATE cxx_std_20)
add_test(NAME artemis_test_switch_motion_policy COMMAND artemis_test_switch_motion_policy)

# Keep portable unit tests independent from libnx/Borealis/deko3d so they can
# run on standard Linux CI.
