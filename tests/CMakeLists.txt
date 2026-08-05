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

include(cmake/integration_tests.cmake)
include(cmake/core_tests.cmake)
include(cmake/module_tests.cmake)
include(cmake/urdf_tests.cmake)
include(cmake/load_tests.cmake)

foreach(stem IN ITEMS arg_scan cmake_listfile_scope)
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

if(TARGET cmake_listfile_scope_test)
    target_compile_definitions(cmake_listfile_scope_test PRIVATE
        MEIOS_REPOSITORY_ROOT="${meios_SOURCE_DIR}")
endif()

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
    foreach(stem IN ITEMS cli_verbs cli_validate cli_tree cli_complete cli_table_drift cli_eval_backend
            cmake_eval_vocabulary)
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

    # This stem alone reads the installed CMake modules and drives the built binary; the
    # other CLI stems have no use for either and would gain a coupling to a directory they
    # never open.
    if(TARGET cmake_eval_vocabulary_test)
        target_compile_definitions(cmake_eval_vocabulary_test PRIVATE
            MEIOS_CMAKE_MODULE_DIR="${meios_SOURCE_DIR}/cmake")
        if(TARGET meios)
            target_compile_definitions(cmake_eval_vocabulary_test PRIVATE
                MEIOS_CLI_BINARY="$<TARGET_FILE:meios>")
        endif()
    endif()
endif()

include(cmake/enrichment_tests.cmake)
include(cmake/eval_python_tests.cmake)
include(cmake/property_tests.cmake)

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/compile/fail)
    add_subdirectory(compile/fail)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/compile/pass)
    add_subdirectory(compile/pass)
endif()

# The MEIOS_BUILD_TESTS conjunct is load-bearing: this directory is also entered under
# MEIOS_BUILD_PROPERTY_TESTS alone, and those jobs must not pick up cases that each run a
# sub-configure.
if(MEIOS_BUILD_TESTS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/integration/cmake)
    add_subdirectory(integration/cmake)
endif()

include(cmake/acquisition_tests.cmake)
