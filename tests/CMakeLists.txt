cmake_minimum_required(VERSION 3.20)
project(ArtemisSwitchBenchmarkTests LANGUAGES CXX)

enable_testing()
add_executable(artemis_benchmark_tests
    benchmark_tests.cpp
    ../app/src/benchmark/BenchmarkAccumulator.cpp)
target_include_directories(artemis_benchmark_tests PRIVATE ../app/src)
target_compile_features(artemis_benchmark_tests PRIVATE cxx_std_20)
add_test(NAME artemis_benchmark_tests COMMAND artemis_benchmark_tests)
