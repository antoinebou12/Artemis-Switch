# Rewrites the wg-nx/lwIP implementation embedded in libnetbird.a into a
# private symbol namespace. NetBird's public API and ordinary libc/libnx
# references remain untouched.

foreach (_required NETBIRD_ARCHIVE NETBIRD_AR NETBIRD_NM NETBIRD_OBJCOPY)
    if (NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
        message(FATAL_ERROR "${_required} is required for NetBird symbol isolation")
    endif ()
endforeach ()

if (NOT EXISTS "${NETBIRD_ARCHIVE}")
    message(FATAL_ERROR "NetBird archive not found: ${NETBIRD_ARCHIVE}")
endif ()

# Exact inventory for netbird-switch 55d5b04 / wg-nx c137c328. If upstream
# changes the archive layout, stop and review which objects belong to the
# private transport instead of accidentally leaking a second global stack.
set(_transport_members
    blake2s_neon.o platform_switch.o wg_chacha20_neon.o wg_counter.o
    wg_crypto.o wg_noise.o wg_poly1305_neon.o wg_relay.o wg_thread.o
    wireguard.o blake2s.o monocypher.o lwip_init.o lwip_def.o
    lwip_inet_chksum.o lwip_ip.o lwip_mem.o lwip_memp.o lwip_netif.o
    lwip_pbuf.o lwip_raw.o lwip_stats.o lwip_sys.o lwip_timeouts.o
    lwip_tcp.o lwip_tcp_in.o lwip_tcp_out.o lwip_udp.o lwip_dns.o
    lwip_icmp.o lwip_ip4.o lwip_ip4_addr.o lwip_ip4_frag.o sys_arch.o
    wg_netif.o)
set(_netbird_members
    netbird_core.o netbird_api.o wg_config.o netbird_login.o nacl_box.o
    tweetnacl.o h2client.o debug_log.o signal_client.o relay_client.o
    socket_fixed.o)
set(_expected_members ${_transport_members} ${_netbird_members})

execute_process(
    COMMAND "${NETBIRD_AR}" t "${NETBIRD_ARCHIVE}"
    RESULT_VARIABLE _ar_result
    OUTPUT_VARIABLE _member_output
    ERROR_VARIABLE _ar_error)
if (NOT _ar_result EQUAL 0)
    message(FATAL_ERROR "Could not inspect libnetbird.a: ${_ar_error}")
endif ()
string(REPLACE "\r\n" "\n" _member_output "${_member_output}")
string(REPLACE "\n" ";" _actual_members "${_member_output}")
list(FILTER _actual_members EXCLUDE REGEX "^$")
if (NOT "${_actual_members}" STREQUAL "${_expected_members}")
    message(FATAL_ERROR
        "Unexpected libnetbird.a member inventory.\n"
        "Expected: ${_expected_members}\nActual: ${_actual_members}")
endif ()

get_filename_component(_archive_dir "${NETBIRD_ARCHIVE}" DIRECTORY)
set(_extract_dir "${_archive_dir}/namespace-members")
set(_map_file "${_archive_dir}/netbird-symbol-map.txt")
set(_raw_archive "${_archive_dir}/libnetbird.raw.a")
file(REMOVE_RECURSE "${_extract_dir}")
file(MAKE_DIRECTORY "${_extract_dir}")

foreach (_member IN LISTS _transport_members)
    execute_process(
        COMMAND "${NETBIRD_AR}" x "${NETBIRD_ARCHIVE}" "${_member}"
        WORKING_DIRECTORY "${_extract_dir}"
        RESULT_VARIABLE _extract_result
        ERROR_VARIABLE _extract_error)
    if (NOT _extract_result EQUAL 0)
        message(FATAL_ERROR "Could not extract ${_member}: ${_extract_error}")
    endif ()

    execute_process(
        COMMAND "${NETBIRD_NM}" -g --defined-only --format=posix "${_member}"
        WORKING_DIRECTORY "${_extract_dir}"
        RESULT_VARIABLE _nm_result
        OUTPUT_VARIABLE _nm_output
        ERROR_VARIABLE _nm_error)
    if (NOT _nm_result EQUAL 0)
        message(FATAL_ERROR "Could not inspect ${_member}: ${_nm_error}")
    endif ()
    string(REPLACE "\r\n" "\n" _nm_output "${_nm_output}")
    string(REPLACE "\n" ";" _nm_lines "${_nm_output}")
    foreach (_line IN LISTS _nm_lines)
        if (_line MATCHES "^([^ ]+) [A-Za-z] ")
            list(APPEND _transport_symbols "${CMAKE_MATCH_1}")
        endif ()
    endforeach ()
endforeach ()

list(REMOVE_DUPLICATES _transport_symbols)
list(SORT _transport_symbols)
if (NOT _transport_symbols)
    message(FATAL_ERROR "No NetBird transport symbols were found to namespace")
endif ()

file(WRITE "${_map_file}" "")
foreach (_symbol IN LISTS _transport_symbols)
    file(APPEND "${_map_file}"
         "${_symbol} netbird_internal_${_symbol}\n")
endforeach ()

file(RENAME "${NETBIRD_ARCHIVE}" "${_raw_archive}")
execute_process(
    COMMAND "${NETBIRD_OBJCOPY}" "--redefine-syms=${_map_file}"
            "${_raw_archive}" "${NETBIRD_ARCHIVE}"
    RESULT_VARIABLE _objcopy_result
    OUTPUT_VARIABLE _objcopy_output
    ERROR_VARIABLE _objcopy_error)
if (NOT _objcopy_result EQUAL 0)
    message(FATAL_ERROR
        "Could not namespace NetBird transport symbols: ${_objcopy_error}")
endif ()

execute_process(
    COMMAND "${NETBIRD_NM}" -g --defined-only --format=posix
            "${NETBIRD_ARCHIVE}"
    RESULT_VARIABLE _verify_result
    OUTPUT_VARIABLE _verify_output
    ERROR_VARIABLE _verify_error)
if (NOT _verify_result EQUAL 0)
    message(FATAL_ERROR "Could not verify namespaced NetBird archive: ${_verify_error}")
endif ()
foreach (_symbol IN LISTS _transport_symbols)
    if (_verify_output MATCHES "(^|\n)${_symbol} [A-Za-z] ")
        message(FATAL_ERROR "Unnamespaced NetBird transport symbol remains: ${_symbol}")
    endif ()
    if (NOT _verify_output MATCHES "(^|\n)netbird_internal_${_symbol} [A-Za-z] ")
        message(FATAL_ERROR "Namespaced NetBird symbol is missing: ${_symbol}")
    endif ()
endforeach ()
if (NOT _verify_output MATCHES "(^|\n)netbird_init [A-Za-z] ")
    message(FATAL_ERROR "NetBird public API was lost during symbol isolation")
endif ()

file(REMOVE_RECURSE "${_extract_dir}")
list(LENGTH _transport_symbols _transport_symbol_count)
message(STATUS
    "Namespaced ${_transport_symbol_count} NetBird wg-nx/lwIP symbols")
