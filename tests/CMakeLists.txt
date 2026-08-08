cmake_minimum_required(VERSION 3.20)
project(ArtemisStreamProfileTests LANGUAGES CXX)

enable_testing()

add_executable(stream_profile_tests
    stream_profile_tests.cpp
    ../app/src/streaming/SwitchStreamProfile.cpp
)
target_include_directories(stream_profile_tests PRIVATE ../app/src/streaming)
target_compile_features(stream_profile_tests PRIVATE cxx_std_20)
add_test(NAME stream_profile_tests COMMAND stream_profile_tests)
