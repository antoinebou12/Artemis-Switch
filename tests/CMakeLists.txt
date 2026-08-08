cmake_minimum_required(VERSION 3.20)
project(ArtemisVideoScaleTests LANGUAGES CXX)

enable_testing()

add_executable(video_scale_tests
    video_scale_tests.cpp
    ../app/src/video/VideoScale.cpp
)
target_include_directories(video_scale_tests PRIVATE ../app/src/video)
target_compile_features(video_scale_tests PRIVATE cxx_std_20)
add_test(NAME video_scale_tests COMMAND video_scale_tests)
