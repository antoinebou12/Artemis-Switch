function(artemis_generate_tailscale_compatibility output_directory)
    get_filename_component(_repo_root
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
    set(_manifest
        "${_repo_root}/compatibility/tailscale/manifest.json")
    if (NOT EXISTS "${_manifest}")
        message(FATAL_ERROR "Missing Tailscale compatibility manifest")
    endif ()

    file(READ "${_manifest}" _manifest_json)
    string(JSON TS_CANDIDATE_CAPABILITY ERROR_VARIABLE _candidate_error
        GET "${_manifest_json}" candidate_capability_version)
    string(JSON _accepted_type ERROR_VARIABLE _accepted_type_error
        TYPE "${_manifest_json}" accepted_capability_version)
    string(JSON TS_PROTOCOL_REFERENCE_COMMIT ERROR_VARIABLE _commit_error
        GET "${_manifest_json}" protocol_reference_commit)
    if (_candidate_error OR _accepted_type_error OR _commit_error)
        message(FATAL_ERROR
            "Invalid Tailscale compatibility manifest: "
            "${_candidate_error}${_accepted_type_error}${_commit_error}")
    endif ()
    if (_accepted_type STREQUAL "NULL")
        set(TS_ACCEPTED_CAPABILITY 0)
    else ()
        string(JSON TS_ACCEPTED_CAPABILITY ERROR_VARIABLE _accepted_error
            GET "${_manifest_json}" accepted_capability_version)
        if (_accepted_error)
            message(FATAL_ERROR
                "Invalid accepted Tailscale capability: ${_accepted_error}")
        endif ()
    endif ()
    string(LENGTH "${TS_PROTOCOL_REFERENCE_COMMIT}" _commit_length)
    if (TS_CANDIDATE_CAPABILITY LESS 1 OR
        NOT _commit_length EQUAL 40 OR
        TS_PROTOCOL_REFERENCE_COMMIT MATCHES "[^0-9a-f]")
        message(FATAL_ERROR "Invalid Tailscale compatibility values")
    endif ()
    if (TS_ACCEPTED_CAPABILITY GREATER TS_CANDIDATE_CAPABILITY)
        message(FATAL_ERROR
            "Accepted Tailscale capability exceeds the audited candidate")
    endif ()

    file(MAKE_DIRECTORY "${output_directory}")
    configure_file(
        "${_repo_root}/app/src/remote_access/tailscale/TailscaleCompatibility.hpp.in"
        "${output_directory}/TailscaleCompatibility.hpp" @ONLY)
endfunction()
