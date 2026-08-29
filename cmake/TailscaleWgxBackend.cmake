# Builds a private, staged wg-nx copy for the experimental Tailscale provider.
# Every symbol in the archive is rewritten; only app-owned wgx_* wrappers are
# exposed to the rest of Artemis.

foreach (_required TAILSCALE_WGX_SRC TAILSCALE_WGX_STAGE TAILSCALE_WGX_AR
                      TAILSCALE_WGX_NM TAILSCALE_WGX_OBJCOPY TAILSCALE_WGX_PATCH)
    if (NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
        message(FATAL_ERROR "${_required} is required")
    endif ()
endforeach ()

if (TAILSCALE_WGX_STAGE MATCHES " ")
    message(FATAL_ERROR "Tailscale wgx stage path must not contain spaces")
endif ()
get_filename_component(_stage_name "${TAILSCALE_WGX_STAGE}" NAME)
if (NOT _stage_name STREQUAL "wg-nx-tailscale")
    message(FATAL_ERROR
        "Refusing to replace unexpected Tailscale stage: ${TAILSCALE_WGX_STAGE}")
endif ()

file(REMOVE_RECURSE "${TAILSCALE_WGX_STAGE}")
file(MAKE_DIRECTORY "${TAILSCALE_WGX_STAGE}/library")
file(COPY "${TAILSCALE_WGX_SRC}/Makefile" DESTINATION "${TAILSCALE_WGX_STAGE}")
foreach (_dir include src lwip-relay)
    file(COPY "${TAILSCALE_WGX_SRC}/${_dir}"
         DESTINATION "${TAILSCALE_WGX_STAGE}")
endforeach ()
foreach (_dir crypto lwip)
    file(COPY "${TAILSCALE_WGX_SRC}/library/${_dir}"
         DESTINATION "${TAILSCALE_WGX_STAGE}/library")
endforeach ()

# The pinned Switch platform source is checked out with CRLF while reviewed
# overlays are stored with LF. Normalize only the disposable staged copy so
# patch applicability is deterministic without dirtying the submodule.
foreach (_source src/platform_switch.c src/wg_internal.h src/wireguard.c
                 include/wireguard.h)
    file(READ "${TAILSCALE_WGX_STAGE}/${_source}" _source_contents)
    string(REPLACE "\r\n" "\n" _source_contents "${_source_contents}")
    file(WRITE "${TAILSCALE_WGX_STAGE}/${_source}" "${_source_contents}")
endforeach ()

find_program(_git_exe NAMES git REQUIRED)
get_filename_component(_patch_dir "${TAILSCALE_WGX_PATCH}" DIRECTORY)
set(_transport_patch "${_patch_dir}/wg-nx-tailscale-instance-transport.patch")
set(_socket_patch "${_patch_dir}/wg-nx-tailscale-instance-socket.patch")
if (NOT EXISTS "${_transport_patch}" OR NOT EXISTS "${_socket_patch}")
    message(FATAL_ERROR "Missing Tailscale instance transport overlay")
endif ()
foreach (_patch "${TAILSCALE_WGX_PATCH}" "${_transport_patch}"
                "${_socket_patch}")
    execute_process(
        COMMAND "${_git_exe}" apply --recount --ignore-space-change
                --whitespace=nowarn "${_patch}"
        WORKING_DIRECTORY "${TAILSCALE_WGX_STAGE}"
        RESULT_VARIABLE _patch_result
        OUTPUT_VARIABLE _patch_output
        ERROR_VARIABLE _patch_error)
    if (NOT _patch_result EQUAL 0)
        message(FATAL_ERROR
            "Could not apply Tailscale wg-nx overlay ${_patch}: "
            "${_patch_output}${_patch_error}")
    endif ()
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
    WORKING_DIRECTORY "${TAILSCALE_WGX_STAGE}"
    RESULT_VARIABLE _build_result
    OUTPUT_VARIABLE _build_output
    ERROR_VARIABLE _build_error)
if (NOT _build_result EQUAL 0)
    message("${_build_output}")
    message("${_build_error}")
    message(FATAL_ERROR "Tailscale wgx build failed")
endif ()

set(_archive "${TAILSCALE_WGX_STAGE}/libwireguard.a")
set(_output "${TAILSCALE_WGX_STAGE}/libtailscale-wgx.a")
set(_expected_members
    blake2s_neon.o platform_switch.o wg_chacha20_neon.o wg_counter.o
    wg_crypto.o wg_noise.o wg_poly1305_neon.o wg_relay.o wg_thread.o
    wireguard.o blake2s.o monocypher.o lwip_init.o lwip_def.o
    lwip_inet_chksum.o lwip_ip.o lwip_mem.o lwip_memp.o lwip_netif.o
    lwip_pbuf.o lwip_raw.o lwip_stats.o lwip_sys.o lwip_timeouts.o
    lwip_tcp.o lwip_tcp_in.o lwip_tcp_out.o lwip_udp.o lwip_dns.o
    lwip_icmp.o lwip_ip4.o lwip_ip4_addr.o lwip_ip4_frag.o sys_arch.o
    wg_netif.o)
execute_process(COMMAND "${TAILSCALE_WGX_AR}" t "${_archive}"
                RESULT_VARIABLE _inventory_result
                OUTPUT_VARIABLE _inventory_output
                ERROR_VARIABLE _inventory_error)
if (NOT _inventory_result EQUAL 0)
    message(FATAL_ERROR "Could not inspect Tailscale wgx archive: ${_inventory_error}")
endif ()
string(REPLACE "\r\n" "\n" _inventory_output "${_inventory_output}")
string(REPLACE "\n" ";" _actual_members "${_inventory_output}")
list(FILTER _actual_members EXCLUDE REGEX "^$")
if (NOT "${_actual_members}" STREQUAL "${_expected_members}")
    message(FATAL_ERROR
        "Unexpected Tailscale wgx archive members.\nExpected: ${_expected_members}\nActual: ${_actual_members}")
endif ()

execute_process(
    COMMAND "${TAILSCALE_WGX_NM}" -g --defined-only --format=posix "${_archive}"
    RESULT_VARIABLE _nm_result OUTPUT_VARIABLE _nm_output ERROR_VARIABLE _nm_error)
if (NOT _nm_result EQUAL 0)
    message(FATAL_ERROR "Could not inventory Tailscale symbols: ${_nm_error}")
endif ()
string(REPLACE "\r\n" "\n" _nm_output "${_nm_output}")
string(REPLACE "\n" ";" _nm_lines "${_nm_output}")
foreach (_line IN LISTS _nm_lines)
    if (_line MATCHES "^([^ ]+) [A-Za-z] ")
        list(APPEND _symbols "${CMAKE_MATCH_1}")
    endif ()
endforeach ()
list(REMOVE_DUPLICATES _symbols)
list(SORT _symbols)
if (NOT _symbols)
    message(FATAL_ERROR "No Tailscale wgx symbols found")
endif ()

set(_map "${TAILSCALE_WGX_STAGE}/tailscale-wgx-symbol-map.txt")
file(WRITE "${_map}" "")
foreach (_symbol IN LISTS _symbols)
    file(APPEND "${_map}" "${_symbol} tailscale_internal_${_symbol}\n")
endforeach ()
execute_process(
    COMMAND "${TAILSCALE_WGX_OBJCOPY}" "--redefine-syms=${_map}"
            "${_archive}" "${_output}"
    RESULT_VARIABLE _objcopy_result
    OUTPUT_VARIABLE _objcopy_output ERROR_VARIABLE _objcopy_error)
if (NOT _objcopy_result EQUAL 0)
    message(FATAL_ERROR "Could not namespace Tailscale wgx: ${_objcopy_error}")
endif ()

execute_process(
    COMMAND "${TAILSCALE_WGX_NM}" -g --defined-only --format=posix "${_output}"
    RESULT_VARIABLE _verify_result
    OUTPUT_VARIABLE _verify_output ERROR_VARIABLE _verify_error)
if (NOT _verify_result EQUAL 0)
    message(FATAL_ERROR "Could not verify Tailscale wgx: ${_verify_error}")
endif ()
foreach (_symbol IN LISTS _symbols)
    if (_verify_output MATCHES "(^|\n)${_symbol} [A-Za-z] ")
        message(FATAL_ERROR "Unnamespaced Tailscale symbol remains: ${_symbol}")
    endif ()
    if (NOT _verify_output MATCHES "(^|\n)tailscale_internal_${_symbol} [A-Za-z] ")
        message(FATAL_ERROR "Namespaced Tailscale symbol missing: ${_symbol}")
    endif ()
endforeach ()
list(LENGTH _symbols _symbol_count)
message(STATUS "Namespaced ${_symbol_count} Tailscale wg-nx/lwIP/crypto symbols")
