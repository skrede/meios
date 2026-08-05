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
        catch_discover_tests(${stem}_test TEST_PREFIX "${stem}.")
    endif()
endforeach()

# This stem alone drives a record read off a document back out through the bundle writer.
if(TARGET urdf_walk_test)
    target_link_libraries(urdf_walk_test PRIVATE meios::bundle)
endif()

# These stems carry TEST_PREFIX so ctest -R can select them by stem; the urdf stems above
# register their cases under prose names and predate that convention.
foreach(stem IN ITEMS urdf_vocabulary urdf_identity urdf_fields urdf_joint_rules urdf_axis
        urdf_inertia load_into)
    set(prefixed_test_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/${stem}_test.cpp)
    if(EXISTS ${prefixed_test_src})
        add_executable(${stem}_test ${prefixed_test_src})
        target_link_libraries(${stem}_test
            PRIVATE meios::core meios::model meios::io meios::xacro meios::urdf
                Catch2::Catch2WithMain)
        target_compile_definitions(${stem}_test PRIVATE
            MEIOS_URDF_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/urdf")
        meios_enable_coverage(${stem}_test)
        meios_warnings(${stem}_test)
        catch_discover_tests(${stem}_test TEST_PREFIX "${stem}.")
    endif()
endforeach()

# The inertia predicate is a compiled core-private symbol reached through the module-private
# header, so this stem alone reaches into meios-core/src and links the XML library that
# header takes its node handles from.
if(TARGET urdf_inertia_test)
    target_include_directories(urdf_inertia_test PRIVATE
        ${meios_SOURCE_DIR}/lib/meios-core/src
        ${meios_SOURCE_DIR}/lib/meios-core/src/meios/urdf)
    target_link_libraries(urdf_inertia_test PRIVATE pugixml::pugixml)
endif()

# The profile stems are driven by tests/golden/urdf/profile.cases, so they need the
# golden directory alongside the fixture directory the other urdf stems get.
foreach(stem IN ITEMS urdf_profile profile_drift profile_permissive)
    set(profile_test_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/${stem}_test.cpp)
    if(EXISTS ${profile_test_src})
        add_executable(${stem}_test ${profile_test_src})
        target_link_libraries(${stem}_test
            PRIVATE meios::core meios::model meios::io meios::xacro meios::urdf
                Catch2::Catch2WithMain)
        target_compile_definitions(${stem}_test PRIVATE
            MEIOS_URDF_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/urdf"
            MEIOS_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden")
        meios_enable_coverage(${stem}_test)
        meios_warnings(${stem}_test)
        # Catch2 registers a case under its prose name, so the stem is only selectable
        # by regex once it prefixes them.
        catch_discover_tests(${stem}_test TEST_PREFIX "${stem}.")
    endif()
endforeach()

# This stem alone reads the published profile document; the runner opens the rule
# table only, and no other target has any use for the documentation directory.
if(TARGET profile_drift_test)
    target_compile_definitions(profile_drift_test PRIVATE
        MEIOS_DOCS_DIR="${meios_SOURCE_DIR}/docs")
endif()

# The uri stems are driven by tests/golden/urdf/uri.cases, so they take the same pair of
# directories the profile stems take.
foreach(stem IN ITEMS uri_cases uri_drift)
    set(uri_test_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/${stem}_test.cpp)
    if(EXISTS ${uri_test_src})
        add_executable(${stem}_test ${uri_test_src})
        target_link_libraries(${stem}_test
            PRIVATE meios::core meios::model meios::io meios::xacro meios::urdf
                Catch2::Catch2WithMain)
        target_compile_definitions(${stem}_test PRIVATE
            MEIOS_URDF_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/urdf"
            MEIOS_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden")
        meios_enable_coverage(${stem}_test)
        meios_warnings(${stem}_test)
        catch_discover_tests(${stem}_test TEST_PREFIX "${stem}.")
    endif()
endforeach()

# This stem alone reads the published uri document, exactly as the profile drift stem reads
# the profile one.
if(TARGET uri_drift_test)
    target_compile_definitions(uri_drift_test PRIVATE
        MEIOS_DOCS_DIR="${meios_SOURCE_DIR}/docs")
endif()

# The uri classifier is a compiled core-private symbol reached through a module-private
# header, so this stem alone reaches into meios-core/src.
if(TARGET uri_cases_test)
    target_include_directories(uri_cases_test PRIVATE
        ${meios_SOURCE_DIR}/lib/meios-core/src)
endif()

# The frame reference is data a consumer diffs against rather than a fixture the library
# reads, so this stem needs the golden directory and links no meios target at all.
set(frames_test_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/urdf_frames_test.cpp)
if(EXISTS ${frames_test_src})
    add_executable(urdf_frames_test ${frames_test_src})
    target_link_libraries(urdf_frames_test PRIVATE Catch2::Catch2WithMain)
    target_compile_definitions(urdf_frames_test PRIVATE
        MEIOS_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden")
    meios_enable_coverage(urdf_frames_test)
    meios_warnings(urdf_frames_test)
    catch_discover_tests(urdf_frames_test TEST_PREFIX "urdf_frames.")
endif()
