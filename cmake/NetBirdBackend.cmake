# Builds libnetbird.a + handle_full.o from the pinned netbird-switch submodule.
#
# The upstream Makefile derives every path from $(CURDIR), so it cannot build a
# source tree whose path contains spaces -- which the default Artemis checkout
# ("New project") does. Rather than patch the vendored Makefile, stage the
# sources into a space-free directory and build there.
#
# Invoked in script mode: cmake -DNETBIRD_SRC=... -DNETBIRD_STAGE=... -P this

if (NOT DEFINED NETBIRD_SRC OR NOT DEFINED NETBIRD_STAGE)
    message(FATAL_ERROR "NETBIRD_SRC and NETBIRD_STAGE are required")
endif ()

if (NOT EXISTS "${NETBIRD_SRC}/Makefile")
    message(FATAL_ERROR "NetBird sources not found at ${NETBIRD_SRC}")
endif ()

if (NETBIRD_STAGE MATCHES " ")
    message(FATAL_ERROR
        "ARTEMIS_VPN_STAGE_ROOT must not contain spaces (got: ${NETBIRD_STAGE})")
endif ()

message(STATUS "Staging NetBird sources into ${NETBIRD_STAGE}")

# Always rebuild from a clean staged copy. Reusing a previously patched source
# tree can silently keep an older patch revision when the patch file changes.
get_filename_component(_netbird_stage_name "${NETBIRD_STAGE}" NAME)
if (NOT _netbird_stage_name STREQUAL "netbird-switch")
    message(FATAL_ERROR
        "Refusing to replace unexpected NetBird stage directory: ${NETBIRD_STAGE}")
endif ()
file(REMOVE_RECURSE "${NETBIRD_STAGE}")

# nghttp2 is a large submodule that the library build does not reference, so it
# is deliberately left out of the copy.
file(MAKE_DIRECTORY "${NETBIRD_STAGE}/library")
foreach (_file Makefile handle_full.c socket_fixed.c)
    file(COPY "${NETBIRD_SRC}/${_file}" DESTINATION "${NETBIRD_STAGE}")
endforeach ()
foreach (_dir include source)
    file(COPY "${NETBIRD_SRC}/${_dir}" DESTINATION "${NETBIRD_STAGE}")
endforeach ()
file(COPY "${NETBIRD_SRC}/library/wg-nx" DESTINATION "${NETBIRD_STAGE}/library")

# Artemis fixes for the pinned backend. The submodule stays clean; patches are
# applied to the staged copy, exactly like the borealis and common-C patches.
set(_netbird_patches
    "${CMAKE_CURRENT_LIST_DIR}/../patches/netbird-switch-relay-use-after-free.patch"
    "${CMAKE_CURRENT_LIST_DIR}/../patches/netbird-switch-proxy-reliability.patch"
    "${CMAKE_CURRENT_LIST_DIR}/../patches/netbird-switch-signal-stream.patch")

find_program(_git_exe NAMES git REQUIRED)
execute_process(
    COMMAND "${_git_exe}" init --quiet
    WORKING_DIRECTORY "${NETBIRD_STAGE}"
    RESULT_VARIABLE _git_init)
if (NOT _git_init EQUAL 0)
    message(FATAL_ERROR "Failed to initialize NetBird patch workspace")
endif ()

foreach (_patch IN LISTS _netbird_patches)
    get_filename_component(_patch_name "${_patch}" NAME)
    execute_process(
        COMMAND "${_git_exe}" apply --check "${_patch}"
        WORKING_DIRECTORY "${NETBIRD_STAGE}"
        RESULT_VARIABLE _patch_check
        OUTPUT_QUIET ERROR_QUIET)
    if (_patch_check EQUAL 0)
        execute_process(
            COMMAND "${_git_exe}" apply "${_patch}"
            WORKING_DIRECTORY "${NETBIRD_STAGE}"
            RESULT_VARIABLE _patch_result)
        if (NOT _patch_result EQUAL 0)
            message(FATAL_ERROR "Failed to apply NetBird patch ${_patch_name}")
        endif ()
        message(STATUS "Applied NetBird patch ${_patch_name}")
    else ()
        execute_process(
            COMMAND "${_git_exe}" apply --reverse --check "${_patch}"
            WORKING_DIRECTORY "${NETBIRD_STAGE}"
            RESULT_VARIABLE _already_applied
            OUTPUT_QUIET ERROR_QUIET)
        if (NOT _already_applied EQUAL 0)
            message(FATAL_ERROR
                    "NetBird patch ${_patch_name} no longer applies to the pinned submodule")
        endif ()
    endif ()
endforeach ()

find_program(_make_exe NAMES make gmake REQUIRED)

include(ProcessorCount)
ProcessorCount(_jobs)
if (_jobs EQUAL 0)
    set(_jobs 4)
endif ()

message(STATUS "Building NetBird backend (-j${_jobs})")

# devkitA64's linker writes temporaries into $TMP; the Windows default is not
# writable from the MSYS2 shell, so point it somewhere that is.
execute_process(
    COMMAND ${CMAKE_COMMAND} -E env TMP=/tmp TEMP=/tmp TMPDIR=/tmp
            "${_make_exe}" libnetbird.a handle_full.o -j${_jobs}
    WORKING_DIRECTORY "${NETBIRD_STAGE}"
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr)

if (NOT _result EQUAL 0)
    message("${_stdout}")
    message("${_stderr}")
    message(FATAL_ERROR "NetBird backend build failed (exit ${_result})")
endif ()

foreach (_artifact libnetbird.a handle_full.o)
    if (NOT EXISTS "${NETBIRD_STAGE}/${_artifact}")
        message(FATAL_ERROR
            "NetBird build reported success but ${_artifact} is missing")
    endif ()
endforeach ()

message(STATUS "NetBird backend ready: ${NETBIRD_STAGE}/libnetbird.a")
