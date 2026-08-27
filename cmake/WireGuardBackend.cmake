# Builds the standalone wg-nx archive from the independently pinned submodule.
#
# The upstream Makefile derives paths from CURDIR, so stage the required source
# directories into the same space-free VPN build root used by NetBird.

if (NOT DEFINED WIREGUARD_SRC OR NOT DEFINED WIREGUARD_STAGE)
    message(FATAL_ERROR "WIREGUARD_SRC and WIREGUARD_STAGE are required")
endif ()

if (NOT EXISTS "${WIREGUARD_SRC}/Makefile" OR
    NOT EXISTS "${WIREGUARD_SRC}/lwip-relay/src/wg_lwip_relay.cpp")
    message(FATAL_ERROR "Standalone wg-nx sources are not initialized at ${WIREGUARD_SRC}")
endif ()

if (NOT EXISTS "${WIREGUARD_SRC}/library/lwip/src/include/lwip/netif.h")
    message(FATAL_ERROR "Standalone wg-nx lwIP submodule is not initialized")
endif ()

if (WIREGUARD_STAGE MATCHES " ")
    message(FATAL_ERROR
        "ARTEMIS_VPN_STAGE_ROOT must not contain spaces (got: ${WIREGUARD_STAGE})")
endif ()

get_filename_component(_wireguard_stage_name "${WIREGUARD_STAGE}" NAME)
if (NOT _wireguard_stage_name STREQUAL "wg-nx-standalone")
    message(FATAL_ERROR
        "Refusing to replace unexpected WireGuard stage directory: ${WIREGUARD_STAGE}")
endif ()

message(STATUS "Staging standalone wg-nx sources into ${WIREGUARD_STAGE}")
file(REMOVE_RECURSE "${WIREGUARD_STAGE}")
file(MAKE_DIRECTORY "${WIREGUARD_STAGE}/library")
file(COPY "${WIREGUARD_SRC}/Makefile" DESTINATION "${WIREGUARD_STAGE}")
foreach (_dir include src lwip-relay)
    file(COPY "${WIREGUARD_SRC}/${_dir}" DESTINATION "${WIREGUARD_STAGE}")
endforeach ()
foreach (_dir crypto lwip)
    file(COPY "${WIREGUARD_SRC}/library/${_dir}"
         DESTINATION "${WIREGUARD_STAGE}/library")
endforeach ()

find_program(_make_exe NAMES make gmake REQUIRED)
include(ProcessorCount)
ProcessorCount(_jobs)
if (_jobs EQUAL 0)
    set(_jobs 4)
endif ()

execute_process(
    COMMAND ${CMAKE_COMMAND} -E env TMP=/tmp TEMP=/tmp TMPDIR=/tmp
            "${_make_exe}" all -j${_jobs}
    WORKING_DIRECTORY "${WIREGUARD_STAGE}"
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr)

if (NOT _result EQUAL 0)
    message("${_stdout}")
    message("${_stderr}")
    message(FATAL_ERROR "Standalone wg-nx build failed (exit ${_result})")
endif ()

if (NOT EXISTS "${WIREGUARD_STAGE}/libwireguard.a")
    message(FATAL_ERROR "Standalone wg-nx build succeeded without libwireguard.a")
endif ()

message(STATUS "Standalone WireGuard backend ready: ${WIREGUARD_STAGE}/libwireguard.a")
