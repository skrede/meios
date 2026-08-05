foreach(stem IN ITEMS urdf_diagnostics urdf_realfixture)
    set(urdf_integration_src ${CMAKE_CURRENT_SOURCE_DIR}/integration/${stem}_test.cpp)
    if(EXISTS ${urdf_integration_src})
        add_executable(${stem}_test ${urdf_integration_src})
        target_link_libraries(${stem}_test
            PRIVATE meios::core meios::model meios::io meios::xacro meios::urdf
                Catch2::Catch2WithMain)
        target_compile_definitions(${stem}_test PRIVATE
            MEIOS_URDF_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/urdf")
        meios_enable_coverage(${stem}_test)
        meios_warnings(${stem}_test)
        catch_discover_tests(${stem}_test)
    endif()
endforeach()

# The corpus test consumes the fetched, pinned descriptions. corpus.cmake runs the
# configure-time license check unconditionally and resolves the fetch OUT_DIRs when
# MEIOS_FETCH_CORPUS is ON; the test target registers only then, so a default
# configure neither hits the network nor requires the corpus dirs.
include(${meios_SOURCE_DIR}/cmake/corpus.cmake)
set(urdf_corpus_src ${CMAKE_CURRENT_SOURCE_DIR}/integration/urdf_corpus_test.cpp)
if(MEIOS_FETCH_CORPUS AND EXISTS ${urdf_corpus_src})
    add_executable(urdf_corpus_test ${urdf_corpus_src})
    target_link_libraries(urdf_corpus_test
        PRIVATE meios::core meios::model meios::io meios::xacro meios::urdf
            Catch2::Catch2WithMain)
    string(REPLACE ";" "|" _corpus_documents "${MEIOS_CORPUS_DOCUMENTS}")
    target_compile_definitions(urdf_corpus_test PRIVATE
        MEIOS_CORPUS_DIR="${MEIOS_CORPUS_KUKA_DIR}"
        MEIOS_CORPUS_KNOWN_GOOD="${MEIOS_CORPUS_KNOWN_GOOD}"
        MEIOS_CORPUS_FAULTY="${MEIOS_CORPUS_FAULTY}"
        MEIOS_CORPUS_FAULTY_LINE=${MEIOS_CORPUS_FAULTY_LINE}
        MEIOS_CORPUS_DOCUMENTS="${_corpus_documents}"
        MEIOS_CORPUS_DOCUMENT_COUNT=${MEIOS_CORPUS_DOCUMENT_COUNT})
    if(TARGET meios_eval-python)
        target_link_libraries(urdf_corpus_test PRIVATE meios::eval-python)
        target_compile_definitions(urdf_corpus_test PRIVATE MEIOS_CORPUS_EVAL_PYTHON)
    endif()
    meios_enable_coverage(urdf_corpus_test)
    meios_warnings(urdf_corpus_test)
    # Without the prefix the ctest names are the Catch2 case titles, which the regex
    # CI selects the corpus by matches none of: the leg then reports success having
    # run nothing at all.
    catch_discover_tests(urdf_corpus_test TEST_PREFIX "urdf_corpus.")
endif()

# The flatten instrument asserts byte identity of flatten output against a
# committed baseline derived from the pinned known-good description; it registers
# only under MEIOS_FETCH_CORPUS so the pinned source is present. TEST_PREFIX names
# the ctest cases flatten_instrument.* so CI can select them by regex.
set(urdf_flatten_src ${CMAKE_CURRENT_SOURCE_DIR}/integration/urdf_flatten_instrument_test.cpp)
if(MEIOS_FETCH_CORPUS AND EXISTS ${urdf_flatten_src})
    add_executable(flatten_instrument_test ${urdf_flatten_src})
    target_link_libraries(flatten_instrument_test
        PRIVATE meios::core meios::model meios::io meios::xacro meios::urdf meios::bundle
            Catch2::Catch2WithMain)
    target_compile_definitions(flatten_instrument_test PRIVATE
        MEIOS_CORPUS_DIR="${MEIOS_CORPUS_KUKA_DIR}"
        MEIOS_CORPUS_KNOWN_GOOD="${MEIOS_CORPUS_KNOWN_GOOD}"
        MEIOS_FLATTEN_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden/flatten")
    meios_enable_coverage(flatten_instrument_test)
    meios_warnings(flatten_instrument_test)
    catch_discover_tests(flatten_instrument_test TEST_PREFIX "flatten_instrument.")
endif()

# A separate target rather than another case in the instrument above: that one compares
# against a pinned upstream description and so registers only under the corpus fetch, which
# would keep a guard written over a small local fixture from ever compiling in a default
# configure.
set(urdf_flatten_resolution_src
    ${CMAKE_CURRENT_SOURCE_DIR}/integration/urdf_flatten_resolution_test.cpp)
if(EXISTS ${urdf_flatten_resolution_src})
    add_executable(urdf_flatten_resolution_test ${urdf_flatten_resolution_src})
    target_link_libraries(urdf_flatten_resolution_test
        PRIVATE meios::core meios::model meios::io meios::xacro meios::urdf meios::bundle
            Catch2::Catch2WithMain)
    target_compile_definitions(urdf_flatten_resolution_test PRIVATE
        MEIOS_URDF_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/urdf"
        MEIOS_FLATTEN_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden/flatten")
    meios_enable_coverage(urdf_flatten_resolution_test)
    meios_warnings(urdf_flatten_resolution_test)
    catch_discover_tests(urdf_flatten_resolution_test TEST_PREFIX "flatten_resolution.")
endif()
