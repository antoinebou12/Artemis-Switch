cmake_minimum_required(VERSION 3.20)
project(ArtemisHostCapabilityTests LANGUAGES CXX)

enable_testing()

add_executable(host_capability_tests
    host_capability_tests.cpp
    ../app/src/host/HostCapabilities.cpp
)
target_include_directories(host_capability_tests PRIVATE ../app/src/host)
target_compile_features(host_capability_tests PRIVATE cxx_std_20)
add_test(NAME host_capability_tests COMMAND host_capability_tests)
