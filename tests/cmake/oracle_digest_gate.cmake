include_guard(GLOBAL)

# The upstream measurements under golden/oracle are the authority for every rendering, coercion and
# unit-tag rule the native evaluator implements. A recorded value changes only by rerunning the
# oracle against the pinned upstream, never by editing the expectation to suit new code, so the
# manifest is recomputed here and fails closed in both directions: a listed record that is missing
# or has drifted, and a record present but absent from the manifest. It runs at configure and,
# so its refusals can be exercised over a scratch copy, as a script.
function(meios_check_oracle_records oracle_dir out_rows)
    file(STRINGS ${oracle_dir}/RECORDS.sha256 manifest)
    set(listed "")
    set(rows 0)
    foreach(row IN LISTS manifest)
        if(row MATCHES "^#")
            continue()
        endif()
        string(REGEX REPLACE "\t.*$" "" recorded "${row}")
        string(REGEX REPLACE "^[^\t]*\t" "" name "${row}")
        if(NOT EXISTS ${oracle_dir}/${name})
            message(FATAL_ERROR
                "oracle: record '${name}' is listed in RECORDS.sha256 but is not on disk. "
                "Rerun tests/tools/oracle/record_upstream.py rather than editing the manifest.")
        endif()
        file(SHA256 ${oracle_dir}/${name} measured)
        if(NOT measured STREQUAL recorded)
            message(FATAL_ERROR
                "oracle: record '${name}' does not match its recorded digest. Upstream's own behavior "
                "is the authority here: a record changes only by rerunning "
                "tests/tools/oracle/record_upstream.py, never by editing an expected value.")
        endif()
        list(APPEND listed ${name})
        file(STRINGS ${oracle_dir}/${name} measured_rows REGEX "^[^#]")
        list(LENGTH measured_rows measured_count)
        math(EXPR rows "${rows} + ${measured_count}")
    endforeach()
    _meios_refuse_unlisted_records(${oracle_dir} "${listed}")
    set(${out_rows} ${rows} PARENT_SCOPE)
endfunction()

# A script run has no build system to re-run it, so only a configure asks for the glob to be
# re-checked.
function(_meios_refuse_unlisted_records oracle_dir listed)
    if(CMAKE_SCRIPT_MODE_FILE)
        file(GLOB present RELATIVE ${oracle_dir} ${oracle_dir}/*.cases)
    else()
        file(GLOB present CONFIGURE_DEPENDS RELATIVE ${oracle_dir} ${oracle_dir}/*.cases)
    endif()
    foreach(name IN LISTS present)
        if(NOT name IN_LIST listed)
            message(FATAL_ERROR
                "oracle: record '${name}' is on disk but carries no digest in RECORDS.sha256. "
                "A record outside the manifest is an unauthorized expectation.")
        endif()
    endforeach()
endfunction()
