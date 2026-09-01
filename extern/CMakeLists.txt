add_subdirectory("${BOREALIS_LIBRARY}" borealis-patched-build)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

set(BUILD_SHARED_LIBS OFF)

if (USE_MBEDTLS_CRYPTO)
    set(USE_MBEDTLS ON)
endif ()

find_package(Git REQUIRED)
set(ARTEMIS_COMMON_C_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/moonlight-common-c")
set(ARTEMIS_COMMON_C_PATCHED "${CMAKE_CURRENT_BINARY_DIR}/moonlight-common-c-patched")
set(ARTEMIS_COMMON_C_PATCHES
    "${CMAKE_CURRENT_SOURCE_DIR}/../patches/moonlight-common-c-apollo-server-command.patch"
    "${CMAKE_CURRENT_SOURCE_DIR}/../patches/moonlight-common-c-loopback-private.patch")

# Keep the pinned dependency pristine. Artemis patches are applied only to a
# build-directory copy and are re-checked on every configure.
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
foreach(ARTEMIS_COMMON_C_PATCH IN LISTS ARTEMIS_COMMON_C_PATCHES)
    get_filename_component(ARTEMIS_COMMON_C_PATCH_NAME
                           "${ARTEMIS_COMMON_C_PATCH}" NAME)
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
            message(FATAL_ERROR
                    "Failed to apply common-C patch ${ARTEMIS_COMMON_C_PATCH_NAME}")
        endif()
    else()
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" apply --reverse --check "${ARTEMIS_COMMON_C_PATCH}"
            WORKING_DIRECTORY "${ARTEMIS_COMMON_C_PATCHED}"
            RESULT_VARIABLE ARTEMIS_PATCH_ALREADY_APPLIED
            OUTPUT_QUIET ERROR_QUIET)
        if(NOT ARTEMIS_PATCH_ALREADY_APPLIED EQUAL 0)
            message(FATAL_ERROR
                    "common-C patch ${ARTEMIS_COMMON_C_PATCH_NAME} no longer applies")
        endif()
    endif()
endforeach()
add_subdirectory("${ARTEMIS_COMMON_C_PATCHED}" moonlight-common-c-patched-build)

if (NOT MULTICAST_DISABLED)
    set(MDNS_BUILD_EXAMPLE OFF)
    add_subdirectory(mdns)
endif ()

# NetBird is a per-application userspace VPN on Horizon. The pinned library
# owns NetBird login/sync, relay transport, wg-nx, lwIP and the localhost
# TCP/UDP proxy. Artemis only selects a peer and redirects GameStream's initial
# gs_init() address to the proxy. This mirrors jmpangilinan/Moonlight-Switch.
if (PLATFORM_SWITCH)
    set(ARTEMIS_NETBIRD_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/netbird-switch")
    set(ARTEMIS_NETBIRD_LIBRARY "${ARTEMIS_NETBIRD_ROOT}/libnetbird.a")
    set(ARTEMIS_NETBIRD_FD_FIX "${ARTEMIS_NETBIRD_ROOT}/handle_full.o")

    if (NOT EXISTS "${ARTEMIS_NETBIRD_ROOT}/Makefile")
        message(FATAL_ERROR
            "NetBird Switch dependency is missing. Clone Artemis with --recursive "
            "or run: git submodule update --init --recursive extern/netbird-switch")
    endif ()

    find_program(ARTEMIS_BASH_EXECUTABLE bash)
    if (NOT ARTEMIS_BASH_EXECUTABLE)
        message(FATAL_ERROR
            "bash is required to build extern/netbird-switch (use devkitPro MSYS2 on Windows)")
    endif ()

    add_custom_command(
        OUTPUT "${ARTEMIS_NETBIRD_LIBRARY}" "${ARTEMIS_NETBIRD_FD_FIX}"
        COMMAND ${CMAKE_COMMAND} -E env
                TMP=/tmp TEMP=/tmp TMPDIR=/tmp
                "${ARTEMIS_BASH_EXECUTABLE}" ./build.sh
        WORKING_DIRECTORY "${ARTEMIS_NETBIRD_ROOT}"
        DEPENDS "${ARTEMIS_NETBIRD_ROOT}/build.sh"
                "${ARTEMIS_NETBIRD_ROOT}/Makefile"
        COMMENT "Building NetBird userspace VPN for Switch"
        VERBATIM)
    add_custom_target(artemis_netbird_build
        DEPENDS "${ARTEMIS_NETBIRD_LIBRARY}" "${ARTEMIS_NETBIRD_FD_FIX}")

    add_library(artemis_netbird INTERFACE)
    add_dependencies(artemis_netbird artemis_netbird_build)
    target_include_directories(artemis_netbird INTERFACE
        "${ARTEMIS_NETBIRD_ROOT}/include")

    # handle_full.o must be placed ahead of the normal toolchain libraries. It
    # replaces newlib's fd-table handling and fixes concurrent close/I/O races.
    # --wrap redirects only gs_init() calls; the original implementation is
    # reached as __real_<mangled-name> from NetBirdManager.cpp.
    target_link_options(artemis_netbird INTERFACE
        "${ARTEMIS_NETBIRD_FD_FIX}"
        "-Wl,--wrap=_Z7gs_initP12_SERVER_DATANSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE")

    # Keep the dependency order documented by netbird-switch. libcurl is
    # repeated intentionally here because static-library resolution is ordered.
    target_link_libraries(artemis_netbird INTERFACE
        "${ARTEMIS_NETBIRD_LIBRARY}"
        "${CMAKE_SOURCE_DIR}/lib/switch/libcurl.a"
        mbedtls
        mbedx509
        mbedcrypto
        z
        nx)

    # The parent links APP_PLATFORM_LIB after creating the Moonlight target, so
    # the interface target also contributes headers and link options to sources.
    set(APP_PLATFORM_LIB artemis_netbird ${APP_PLATFORM_LIB} PARENT_SCOPE)
endif ()
