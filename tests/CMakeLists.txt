cmake_minimum_required(VERSION 3.20)
project(ArtemisSwitchUnitTests LANGUAGES CXX)

enable_testing()

add_executable(artemis_test_smoke smoke_test.cpp)
target_compile_features(artemis_test_smoke PRIVATE cxx_std_20)
add_test(NAME artemis_test_smoke COMMAND artemis_test_smoke)

add_executable(artemis_test_advanced_stream_options
    advanced_stream_options_test.cpp
    ../app/src/features/stream/AdvancedStreamOptions.cpp)
target_compile_features(artemis_test_advanced_stream_options PRIVATE cxx_std_20)
add_test(NAME artemis_test_advanced_stream_options COMMAND artemis_test_advanced_stream_options)

# Keep portable unit tests independent from libnx/Borealis/deko3d so they can
# run on standard Linux CI.
