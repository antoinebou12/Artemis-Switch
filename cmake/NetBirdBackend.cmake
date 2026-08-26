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
