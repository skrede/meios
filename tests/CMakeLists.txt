include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG e35b484d7c083a7f949ce5ec555ff04b5221a3c5  # Catch2 v3.15.2
    SYSTEM
    FIND_PACKAGE_ARGS 3 NAMES Catch2
)
FetchContent_MakeAvailable(Catch2)

# rapidcheck publishes no tags, so an immutable commit is the only stable pin.
# RC_ENABLE_CATCH would also add rapidcheck's vendored ext/catch (a Catch2 v2.4.2
# target) which collides with the Catch2 v3 fetched above; RC_INSTALL_ALL_EXTRAS
# yields the same rapidcheck_catch bridge target without that vendored copy.
if(MEIOS_BUILD_PROPERTY_TESTS)
    set(RC_ENABLE_CATCH OFF)
    set(RC_INSTALL_ALL_EXTRAS ON)
    FetchContent_Declare(
        rapidcheck
        GIT_REPOSITORY https://github.com/emil-e/rapidcheck.git
        GIT_TAG b2d9ed2dddefc4b84318d664b4f221eb792d89c7
        SYSTEM
    )
    FetchContent_MakeAvailable(rapidcheck)
endif()

# A fetched Catch2 keeps its extras in a child scope; a found one is added by its
# own config. Cover both before pulling in catch_discover_tests.
if(catch2_SOURCE_DIR)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
endif()
include(Catch)

function(meios_add_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE meios::core ${ARGN} Catch2::Catch2WithMain)
    meios_enable_coverage(${name})
    meios_warnings(${name})
    catch_discover_tests(${name})
endfunction()

meios_add_test(version_test)
target_compile_definitions(version_test PRIVATE
    MEIOS_CMAKE_PROJECT_VERSION="${meios_VERSION}")

add_executable(integration_test integration/integration_test.cpp)
target_link_libraries(integration_test
    PRIVATE meios::core meios::model Catch2::Catch2WithMain)
meios_enable_coverage(integration_test)
meios_warnings(integration_test)
catch_discover_tests(integration_test)

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
include(${CMAKE_SOURCE_DIR}/cmake/corpus.cmake)
set(urdf_corpus_src ${CMAKE_CURRENT_SOURCE_DIR}/integration/urdf_corpus_test.cpp)
if(MEIOS_FETCH_CORPUS AND EXISTS ${urdf_corpus_src})
    add_executable(urdf_corpus_test ${urdf_corpus_src})
    target_link_libraries(urdf_corpus_test
        PRIVATE meios::core meios::model meios::io meios::xacro meios::urdf
            Catch2::Catch2WithMain)
    target_compile_definitions(urdf_corpus_test PRIVATE
        MEIOS_CORPUS_DIR="${MEIOS_CORPUS_KUKA_DIR}"
        MEIOS_CORPUS_KNOWN_GOOD="${MEIOS_CORPUS_KNOWN_GOOD}"
        MEIOS_CORPUS_FAULTY="${MEIOS_CORPUS_FAULTY}"
        MEIOS_CORPUS_FAULTY_LINE=${MEIOS_CORPUS_FAULTY_LINE})
    if(MEIOS_CORPUS_BREADTH)
        string(REPLACE ";" "|" _corpus_breadth_dirs "${MEIOS_CORPUS_BREADTH_DIRS}")
        target_compile_definitions(urdf_corpus_test PRIVATE
            MEIOS_CORPUS_BREADTH
            MEIOS_CORPUS_BREADTH_DIRS="${_corpus_breadth_dirs}")
    endif()
    meios_enable_coverage(urdf_corpus_test)
    meios_warnings(urdf_corpus_test)
    catch_discover_tests(urdf_corpus_test)
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

foreach(stem IN ITEMS math diag records types sink)
    set(model_test_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/model_${stem}_test.cpp)
    if(EXISTS ${model_test_src})
        add_executable(model_${stem}_test ${model_test_src})
        target_link_libraries(model_${stem}_test
            PRIVATE meios::core meios::model Catch2::Catch2WithMain)
        meios_enable_coverage(model_${stem}_test)
        meios_warnings(model_${stem}_test)
        catch_discover_tests(model_${stem}_test)
    endif()
endforeach()

foreach(stem IN ITEMS world_recorder)
    set(recorder_test_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/${stem}_test.cpp)
    if(EXISTS ${recorder_test_src})
        add_executable(${stem}_test ${recorder_test_src})
        target_link_libraries(${stem}_test
            PRIVATE meios::core meios::model Catch2::Catch2WithMain)
        meios_enable_coverage(${stem}_test)
        meios_warnings(${stem}_test)
        catch_discover_tests(${stem}_test)
    endif()
endforeach()

# The expected vocabulary aliases std::expected where the standard library ships it
# and a C++20 fallback otherwise. Both branches carry the same call surface, so one
# source proves both: expected_std_test pins C++23 to reach std::expected, and
# expected_fallback_test forces the fallback even where std::expected exists.
set(expected_test_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/expected_test.cpp)
if(EXISTS ${expected_test_src})
    add_executable(expected_std_test ${expected_test_src})
    target_compile_features(expected_std_test PRIVATE cxx_std_23)
    target_link_libraries(expected_std_test
        PRIVATE meios::core meios::model Catch2::Catch2WithMain)
    meios_enable_coverage(expected_std_test)
    meios_warnings(expected_std_test)
    catch_discover_tests(expected_std_test TEST_PREFIX "std.")

    add_executable(expected_fallback_test ${expected_test_src})
    target_compile_definitions(expected_fallback_test PRIVATE MEIOS_EXPECTED_FORCE_FALLBACK)
    target_link_libraries(expected_fallback_test
        PRIVATE meios::core meios::model Catch2::Catch2WithMain)
    meios_enable_coverage(expected_fallback_test)
    meios_warnings(expected_fallback_test)
    catch_discover_tests(expected_fallback_test TEST_PREFIX "fallback.")
endif()

# The module unit stems are wired but guarded: a stem becomes a real test the
# moment its source drops in, with no further CMake edit. meios::model is linked
# so a stem reaching the diagnostic seam through a module header compiles.
foreach(stem IN ITEMS
        xacro_eval xacro_structural xacro_subst xacro_budget xacro_arg
        xacro_unsupported xacro_corpus io_concept io_source_stack io_sources)
    set(module_test_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/${stem}_test.cpp)
    if(EXISTS ${module_test_src})
        add_executable(${stem}_test ${module_test_src})
        target_link_libraries(${stem}_test
            PRIVATE meios::core meios::model meios::xacro meios::io
                Catch2::Catch2WithMain)
        if(stem STREQUAL "xacro_corpus" OR stem STREQUAL "xacro_structural")
            target_compile_definitions(${stem}_test PRIVATE
                MEIOS_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden")
        endif()
        meios_enable_coverage(${stem}_test)
        meios_warnings(${stem}_test)
        catch_discover_tests(${stem}_test)
    endif()
endforeach()

foreach(stem IN ITEMS urdf_walk urdf_material urdf_topology urdf_mesh urdf_strict)
    set(urdf_test_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/${stem}_test.cpp)
    if(EXISTS ${urdf_test_src})
        add_executable(${stem}_test ${urdf_test_src})
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

foreach(stem IN ITEMS load_error silent_default overload_resolution)
    set(load_test_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/${stem}_test.cpp)
    if(EXISTS ${load_test_src})
        add_executable(${stem}_test ${load_test_src})
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

foreach(stem IN ITEMS bundle_writer bundle_writer_pkg bundle_scanner)
    set(bundle_test_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/${stem}_test.cpp)
    if(EXISTS ${bundle_test_src})
        add_executable(${stem}_test ${bundle_test_src})
        target_link_libraries(${stem}_test
            PRIVATE meios::core meios::model meios::io meios::urdf meios::bundle
                Catch2::Catch2WithMain)
        target_compile_definitions(${stem}_test PRIVATE
            MEIOS_URDF_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/urdf")
        meios_enable_coverage(${stem}_test)
        meios_warnings(${stem}_test)
        catch_discover_tests(${stem}_test)
    endif()
endforeach()

foreach(stem IN ITEMS urdf_load_sources)
    set(load_sources_src ${CMAKE_CURRENT_SOURCE_DIR}/integration/${stem}_test.cpp)
    if(EXISTS ${load_sources_src})
        add_executable(${stem}_test ${load_sources_src})
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

foreach(stem IN ITEMS arg_scan)
    set(arg_scan_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/${stem}_test.cpp)
    if(EXISTS ${arg_scan_src})
        add_executable(${stem}_test ${arg_scan_src})
        target_link_libraries(${stem}_test
            PRIVATE meios::core meios::model meios::xacro Catch2::Catch2WithMain)
        meios_enable_coverage(${stem}_test)
        meios_warnings(${stem}_test)
        catch_discover_tests(${stem}_test)
    endif()
endforeach()

foreach(stem IN ITEMS completion_emit)
    set(completion_test_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/${stem}_test.cpp)
    if(EXISTS ${completion_test_src})
        add_executable(${stem}_test ${completion_test_src})
        target_link_libraries(${stem}_test
            PRIVATE meios::core meios::model meios::completion
                Catch2::Catch2WithMain)
        target_compile_definitions(${stem}_test PRIVATE
            MEIOS_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden")
        meios_enable_coverage(${stem}_test)
        meios_warnings(${stem}_test)
        catch_discover_tests(${stem}_test)
    endif()
endforeach()

# The CLI stems reach the tool surface only through the meios_cli support lib,
# which carries CLI11 PRIVATE. The TARGET guard keeps a MEIOS_BUILD_TOOLS=OFF
# configure green even once these sources land.
if(TARGET meios_cli)
    foreach(stem IN ITEMS cli_verbs cli_validate cli_tree cli_complete cli_table_drift cli_eval_backend)
        set(cli_test_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/${stem}_test.cpp)
        if(EXISTS ${cli_test_src})
            add_executable(${stem}_test ${cli_test_src})
            target_link_libraries(${stem}_test
                PRIVATE meios_cli meios::urdf meios::bundle meios::completion
                    Catch2::Catch2WithMain)
            target_compile_definitions(${stem}_test PRIVATE
                MEIOS_URDF_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/urdf"
                MEIOS_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden")
            meios_enable_coverage(${stem}_test)
            meios_warnings(${stem}_test)
            catch_discover_tests(${stem}_test)
        endif()
    endforeach()
endif()

# Enrichment unit stems. Each scanner/ros/zip/python stem is gated on both its
# module TARGET and the EXISTS source guard, so a default (all enrichment options
# OFF) configure adds none of them; a stem becomes a live test the moment its
# implementing plan lands the module source and the test .cpp together. The
# eval_policy stem drives the always-on core evaluator, so it carries no TARGET
# guard — only the EXISTS guard keeps it dormant until its source lands.
if(TARGET meios_scan-obj)
    set(scan_obj_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/scan_obj_test.cpp)
    if(EXISTS ${scan_obj_src})
        add_executable(scan_obj_test ${scan_obj_src})
        target_link_libraries(scan_obj_test
            PRIVATE meios::core meios::model meios::io meios::bundle meios::scan-obj
                Catch2::Catch2WithMain)
        target_compile_definitions(scan_obj_test PRIVATE
            MEIOS_ASSET_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
        meios_enable_coverage(scan_obj_test)
        meios_warnings(scan_obj_test)
        catch_discover_tests(scan_obj_test)
    endif()
endif()

if(TARGET meios_scan-stl)
    set(scan_stl_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/scan_stl_test.cpp)
    if(EXISTS ${scan_stl_src})
        add_executable(scan_stl_test ${scan_stl_src})
        target_link_libraries(scan_stl_test
            PRIVATE meios::core meios::model meios::io meios::bundle meios::scan-stl
                Catch2::Catch2WithMain)
        target_compile_definitions(scan_stl_test PRIVATE
            MEIOS_ASSET_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
        meios_enable_coverage(scan_stl_test)
        meios_warnings(scan_stl_test)
        catch_discover_tests(scan_stl_test)
    endif()
endif()

if(TARGET meios_scan-collada)
    set(scan_collada_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/scan_collada_test.cpp)
    if(EXISTS ${scan_collada_src})
        add_executable(scan_collada_test ${scan_collada_src})
        target_link_libraries(scan_collada_test
            PRIVATE meios::core meios::model meios::io meios::bundle meios::scan-collada
                Catch2::Catch2WithMain)
        target_compile_definitions(scan_collada_test PRIVATE
            MEIOS_ASSET_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
        meios_enable_coverage(scan_collada_test)
        meios_warnings(scan_collada_test)
        catch_discover_tests(scan_collada_test)
    endif()
endif()

if(TARGET meios_scan-gltf)
    set(scan_gltf_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/scan_gltf_test.cpp)
    if(EXISTS ${scan_gltf_src})
        add_executable(scan_gltf_test ${scan_gltf_src})
        target_link_libraries(scan_gltf_test
            PRIVATE meios::core meios::model meios::io meios::bundle meios::scan-gltf
                Catch2::Catch2WithMain)
        target_compile_definitions(scan_gltf_test PRIVATE
            MEIOS_ASSET_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
        meios_enable_coverage(scan_gltf_test)
        meios_warnings(scan_gltf_test)
        catch_discover_tests(scan_gltf_test)
    endif()
endif()

if(TARGET meios_ros)
    set(ros_source_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/ros_source_test.cpp)
    if(EXISTS ${ros_source_src})
        add_executable(ros_source_test ${ros_source_src})
        target_link_libraries(ros_source_test
            PRIVATE meios::core meios::model meios::io meios::ros
                Catch2::Catch2WithMain)
        meios_enable_coverage(ros_source_test)
        meios_warnings(ros_source_test)
        catch_discover_tests(ros_source_test)
    endif()
endif()

if(TARGET meios_archive-zip)
    set(zip_writer_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/zip_writer_test.cpp)
    if(EXISTS ${zip_writer_src})
        add_executable(zip_writer_test ${zip_writer_src})
        target_link_libraries(zip_writer_test
            PRIVATE meios::core meios::model meios::io meios::bundle meios::archive-zip
                miniz Catch2::Catch2WithMain)
        target_compile_definitions(zip_writer_test PRIVATE
            MEIOS_URDF_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/urdf")
        meios_enable_coverage(zip_writer_test)
        meios_warnings(zip_writer_test)
        catch_discover_tests(zip_writer_test)
    endif()
endif()

set(eval_policy_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/eval_policy_test.cpp)
if(EXISTS ${eval_policy_src})
    add_executable(eval_policy_test ${eval_policy_src})
    target_link_libraries(eval_policy_test
        PRIVATE meios::core meios::model meios::xacro meios::urdf
            Catch2::Catch2WithMain)
    target_compile_definitions(eval_policy_test PRIVATE
        MEIOS_URDF_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/urdf"
        MEIOS_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden")
    meios_enable_coverage(eval_policy_test)
    meios_warnings(eval_policy_test)
    catch_discover_tests(eval_policy_test)
endif()

if(TARGET meios_eval-python)
    set(eval_python_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/eval_python_test.cpp)
    if(EXISTS ${eval_python_src})
        add_executable(eval_python_test ${eval_python_src})
        target_link_libraries(eval_python_test
            PRIVATE meios::core meios::model meios::io meios::xacro meios::eval-python
                Catch2::Catch2WithMain)
        target_compile_definitions(eval_python_test PRIVATE
            MEIOS_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden")
        meios_enable_coverage(eval_python_test)
        meios_warnings(eval_python_test)
        catch_discover_tests(eval_python_test)
    endif()

    set(urdf_yaml_flatten_src ${CMAKE_CURRENT_SOURCE_DIR}/integration/urdf_yaml_flatten_test.cpp)
    if(EXISTS ${urdf_yaml_flatten_src})
        add_executable(urdf_yaml_flatten_test ${urdf_yaml_flatten_src})
        target_link_libraries(urdf_yaml_flatten_test
            PRIVATE meios::core meios::model meios::io meios::xacro meios::urdf meios::eval-python
                Catch2::Catch2WithMain)
        target_compile_definitions(urdf_yaml_flatten_test PRIVATE
            MEIOS_URDF_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/urdf")
        if(TARGET meios)
            target_compile_definitions(urdf_yaml_flatten_test PRIVATE
                MEIOS_CLI_BINARY="$<TARGET_FILE:meios>")
        endif()
        meios_enable_coverage(urdf_yaml_flatten_test)
        meios_warnings(urdf_yaml_flatten_test)
        catch_discover_tests(urdf_yaml_flatten_test)
    endif()
endif()

if(MEIOS_BUILD_PROPERTY_TESTS)
    set(xacro_eval_prop_src ${CMAKE_CURRENT_SOURCE_DIR}/property/xacro_eval_prop.cpp)
    if(EXISTS ${xacro_eval_prop_src})
        add_executable(xacro_eval_prop ${xacro_eval_prop_src})
        target_link_libraries(xacro_eval_prop
            PRIVATE meios::core meios::model meios::xacro
                rapidcheck rapidcheck_catch Catch2::Catch2WithMain)
        meios_enable_coverage(xacro_eval_prop)
        meios_warnings(xacro_eval_prop)
        catch_discover_tests(xacro_eval_prop)
    endif()

    set(urdf_prop_src ${CMAKE_CURRENT_SOURCE_DIR}/property/urdf_prop.cpp)
    if(EXISTS ${urdf_prop_src})
        add_executable(urdf_prop ${urdf_prop_src})
        target_link_libraries(urdf_prop
            PRIVATE meios::core meios::model meios::io meios::xacro meios::urdf
                rapidcheck rapidcheck_catch Catch2::Catch2WithMain)
        meios_enable_coverage(urdf_prop)
        meios_warnings(urdf_prop)
        catch_discover_tests(urdf_prop)
    endif()

    set(urdf_roundtrip_src ${CMAKE_CURRENT_SOURCE_DIR}/property/urdf_roundtrip_prop.cpp)
    if(EXISTS ${urdf_roundtrip_src})
        add_executable(urdf_roundtrip ${urdf_roundtrip_src})
        target_link_libraries(urdf_roundtrip
            PRIVATE meios::core meios::model meios::io meios::xacro meios::urdf
                meios::bundle rapidcheck rapidcheck_catch Catch2::Catch2WithMain)
        meios_enable_coverage(urdf_roundtrip)
        meios_warnings(urdf_roundtrip)
        catch_discover_tests(urdf_roundtrip)
    endif()
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/compile/fail)
    add_subdirectory(compile/fail)
endif()
