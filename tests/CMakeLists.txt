cmake_minimum_required(VERSION 3.20)
project(ArtemisSwitchUnitTests LANGUAGES CXX)

enable_testing()

add_executable(artemis_test_smoke smoke_test.cpp)
target_compile_features(artemis_test_smoke PRIVATE cxx_std_20)
add_test(NAME artemis_test_smoke COMMAND artemis_test_smoke)

add_executable(artemis_test_performance_lite
    performance_lite_test.cpp
    ../app/src/features/performance/PerformanceLite.cpp)
target_compile_features(artemis_test_performance_lite PRIVATE cxx_std_20)
add_test(NAME artemis_test_performance_lite COMMAND artemis_test_performance_lite)

# Keep portable unit tests independent from libnx/Borealis/deko3d so they can
# run on standard Linux CI.
