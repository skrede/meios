# The upstream measurements under golden/oracle are the authority for every rendering, coercion and
# unit-tag rule the native evaluator implements. A recorded value changes only by rerunning the
# oracle against the pinned upstream, never by editing the expectation to suit new code, so the
# manifest is recomputed here and the configure fails closed in both directions: a listed record
# that is missing or has drifted, and a record present but absent from the manifest.
set(meios_oracle_dir ${CMAKE_CURRENT_SOURCE_DIR}/golden/oracle)
file(STRINGS ${meios_oracle_dir}/RECORDS.sha256 meios_oracle_manifest)
set(meios_oracle_listed "")
set(MEIOS_ORACLE_RECORD_ROWS 0)
foreach(row IN LISTS meios_oracle_manifest)
    if(row MATCHES "^#")
        continue()
    endif()
    string(REGEX REPLACE "\t.*$" "" recorded "${row}")
    string(REGEX REPLACE "^[^\t]*\t" "" name "${row}")
    if(NOT EXISTS ${meios_oracle_dir}/${name})
        message(FATAL_ERROR
            "oracle: record '${name}' is listed in RECORDS.sha256 but is not on disk. "
            "Rerun tests/tools/oracle/record_upstream.py rather than editing the manifest.")
    endif()
    file(SHA256 ${meios_oracle_dir}/${name} measured)
    if(NOT measured STREQUAL recorded)
        message(FATAL_ERROR
            "oracle: record '${name}' does not match its recorded digest. Upstream's own behavior "
            "is the authority here: a record changes only by rerunning "
            "tests/tools/oracle/record_upstream.py, never by editing an expected value.")
    endif()
    list(APPEND meios_oracle_listed ${name})
    file(STRINGS ${meios_oracle_dir}/${name} measured_rows REGEX "^[^#]")
    list(LENGTH measured_rows measured_count)
    math(EXPR MEIOS_ORACLE_RECORD_ROWS "${MEIOS_ORACLE_RECORD_ROWS} + ${measured_count}")
endforeach()

file(GLOB meios_oracle_present CONFIGURE_DEPENDS
     RELATIVE ${meios_oracle_dir} ${meios_oracle_dir}/*.cases)
foreach(name IN LISTS meios_oracle_present)
    if(NOT name IN_LIST meios_oracle_listed)
        message(FATAL_ERROR
            "oracle: record '${name}' is on disk but carries no digest in RECORDS.sha256. "
            "A record outside the manifest is an unauthorized expectation.")
    endif()
endforeach()

# Every stem the native evaluator work will add is registered here behind the EXISTS guard, so a
# stem becomes a live test the moment its source lands and no build edit is needed to make it run.
# A record stem reads only the measurements; an evaluator stem also reaches the lexer, the
# expression parser and the per-load session, all of which are core-private.
function(meios_add_native_test stem kind)
    set(test_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/${stem}_test.cpp)
    if(NOT EXISTS ${test_src})
        return()
    endif()
    add_executable(${stem}_test ${test_src})
    target_link_libraries(${stem}_test PRIVATE meios::core meios::model Catch2::Catch2WithMain)
    target_compile_definitions(${stem}_test PRIVATE
        MEIOS_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden")
    if(NOT kind STREQUAL "record")
        target_include_directories(${stem}_test PRIVATE ${meios_SOURCE_DIR}/lib/meios-core/src)
        target_link_libraries(${stem}_test PRIVATE meios::io meios::xacro meios::urdf)
        target_compile_definitions(${stem}_test PRIVATE
            MEIOS_URDF_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/urdf")
    endif()
    # Registered outside the module guard so the stem still builds and runs its non-module cases in
    # a tree configured without the YAML module.
    if(kind STREQUAL "yaml" AND TARGET meios_yaml)
        target_link_libraries(${stem}_test PRIVATE meios::yaml)
        target_compile_definitions(${stem}_test PRIVATE MEIOS_TEST_HAS_YAML=1)
    endif()
    meios_enable_coverage(${stem}_test)
    meios_warnings(${stem}_test)
    catch_discover_tests(${stem}_test TEST_PREFIX "${stem}.")
endfunction()

foreach(stem IN ITEMS native_oracle_records native_corpus_record)
    meios_add_native_test(${stem} record)
endforeach()

foreach(stem IN ITEMS native_value native_render native_expression native_session native_budget
                      native_branch)
    meios_add_native_test(${stem} evaluator)
endforeach()

foreach(stem IN ITEMS native_yaml native_yaml_resolver)
    meios_add_native_test(${stem} yaml)
endforeach()

# This stem alone reads the project's own listfiles, so the module directory reaches it and no
# other stem gains a coupling to a directory it never opens.
if(TARGET native_corpus_record_test)
    target_compile_definitions(native_corpus_record_test PRIVATE
        MEIOS_CMAKE_MODULE_DIR="${meios_SOURCE_DIR}/cmake")
endif()

# The row total is what keeps a present-but-emptied record set from passing every downstream
# assertion vacuously.
if(TARGET native_oracle_records_test)
    target_compile_definitions(native_oracle_records_test PRIVATE
        MEIOS_ORACLE_RECORD_ROWS=${MEIOS_ORACLE_RECORD_ROWS})
endif()
