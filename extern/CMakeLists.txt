add_subdirectory("${BOREALIS_LIBRARY}" borealis-patched-build)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

set(BUILD_SHARED_LIBS OFF)

if (USE_MBEDTLS_CRYPTO)
    set(USE_MBEDTLS ON)
endif ()

find_package(Git REQUIRED)
set(ARTEMIS_COMMON_C_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/moonlight-common-c")
set(ARTEMIS_COMMON_C_PATCHED "${CMAKE_CURRENT_BINARY_DIR}/moonlight-common-c-patched")
set(ARTEMIS_COMMON_C_PATCH
    "${CMAKE_CURRENT_SOURCE_DIR}/../patches/moonlight-common-c-apollo-server-command.patch")

# Keep the pinned dependency pristine. The narrow Apollo control-channel patch
# is applied only to a build-directory copy and is checked on every configure.
file(COPY "${ARTEMIS_COMMON_C_SOURCE}/"
     DESTINATION "${ARTEMIS_COMMON_C_PATCHED}")
# The submodule's .git indirection points outside the build copy and would make
# `git apply` resolve the wrong repository.
file(REMOVE_RECURSE "${ARTEMIS_COMMON_C_PATCHED}/.git")
execute_process(
    COMMAND "${GIT_EXECUTABLE}" init --quiet
    WORKING_DIRECTORY "${ARTEMIS_COMMON_C_PATCHED}"
    RESULT_VARIABLE ARTEMIS_PATCH_REPO_INIT)
if(NOT ARTEMIS_PATCH_REPO_INIT EQUAL 0)
    message(FATAL_ERROR "Failed to initialize common-C build patch workspace")
endif()
execute_process(
    COMMAND "${GIT_EXECUTABLE}" apply --check "${ARTEMIS_COMMON_C_PATCH}"
    WORKING_DIRECTORY "${ARTEMIS_COMMON_C_PATCHED}"
    RESULT_VARIABLE ARTEMIS_PATCH_CHECK
    OUTPUT_QUIET ERROR_QUIET)
if(ARTEMIS_PATCH_CHECK EQUAL 0)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" apply "${ARTEMIS_COMMON_C_PATCH}"
        WORKING_DIRECTORY "${ARTEMIS_COMMON_C_PATCHED}"
        RESULT_VARIABLE ARTEMIS_PATCH_RESULT)
    if(NOT ARTEMIS_PATCH_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to apply Apollo common-C patch")
    endif()
else()
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" apply --reverse --check "${ARTEMIS_COMMON_C_PATCH}"
        WORKING_DIRECTORY "${ARTEMIS_COMMON_C_PATCHED}"
        RESULT_VARIABLE ARTEMIS_PATCH_ALREADY_APPLIED
        OUTPUT_QUIET ERROR_QUIET)
    if(NOT ARTEMIS_PATCH_ALREADY_APPLIED EQUAL 0)
        message(FATAL_ERROR "Apollo common-C patch no longer applies to the pinned dependency")
    endif()
endif()
add_subdirectory("${ARTEMIS_COMMON_C_PATCHED}" moonlight-common-c-patched-build)

if (NOT MULTICAST_DISABLED)
    set(MDNS_BUILD_EXAMPLE OFF)
    add_subdirectory(mdns)
endif ()
