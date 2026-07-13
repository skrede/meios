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
