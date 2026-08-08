cmake_minimum_required(VERSION 3.20)
project(ArtemisAutoTuneTests LANGUAGES CXX)

enable_testing()

add_executable(auto_tune_tests
    auto_tune_tests.cpp
    ../app/src/benchmark/AutoTune.cpp
    ../app/src/benchmark/AutoTunePlan.cpp
)
target_include_directories(auto_tune_tests PRIVATE ../app/src/benchmark)
target_compile_features(auto_tune_tests PRIVATE cxx_std_20)
add_test(NAME auto_tune_tests COMMAND auto_tune_tests)
