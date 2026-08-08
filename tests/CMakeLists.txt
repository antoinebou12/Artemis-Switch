cmake_minimum_required(VERSION 3.20)
project(ArtemisSwitchUnitTests LANGUAGES CXX)

enable_testing()

add_executable(artemis_test_smoke smoke_test.cpp)
target_compile_features(artemis_test_smoke PRIVATE cxx_std_20)
add_test(NAME artemis_test_smoke COMMAND artemis_test_smoke)

# Feature branches append their portable test targets here. Keep unit-test code
# independent from libnx/Borealis/deko3d so it can run on standard Linux CI.
