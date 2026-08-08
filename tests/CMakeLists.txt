cmake_minimum_required(VERSION 3.20)
project(ArtemisBenchmarkExportTests LANGUAGES CXX)

enable_testing()

add_executable(benchmark_export_tests
    benchmark_export_tests.cpp
    ../app/src/benchmark/BenchmarkExport.cpp
    ../app/src/benchmark/BenchmarkFileStore.cpp
)
target_include_directories(benchmark_export_tests PRIVATE ../app/src/benchmark)
target_compile_features(benchmark_export_tests PRIVATE cxx_std_20)
add_test(NAME benchmark_export_tests COMMAND benchmark_export_tests)
