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
        catch_discover_tests(scan_obj_test TEST_PREFIX "scan_obj.")
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
        catch_discover_tests(scan_stl_test TEST_PREFIX "scan_stl.")
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
        catch_discover_tests(scan_collada_test TEST_PREFIX "scan_collada.")
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
        catch_discover_tests(scan_gltf_test TEST_PREFIX "scan_gltf.")
    endif()
endif()

if(TARGET meios_ros-package)
    set(ros_source_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/ros_source_test.cpp)
    if(EXISTS ${ros_source_src})
        add_executable(ros_source_test ${ros_source_src})
        target_link_libraries(ros_source_test
            PRIVATE meios::core meios::model meios::io meios::ros-package
                Catch2::Catch2WithMain)
        meios_enable_coverage(ros_source_test)
        meios_warnings(ros_source_test)
        catch_discover_tests(ros_source_test TEST_PREFIX "ros_source.")
    endif()

    set(ros_prefix_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/ros_prefix_test.cpp)
    if(EXISTS ${ros_prefix_src})
        add_executable(ros_prefix_test ${ros_prefix_src})
        target_link_libraries(ros_prefix_test
            PRIVATE meios::core meios::model meios::io meios::ros-package
                Catch2::Catch2WithMain)
        meios_enable_coverage(ros_prefix_test)
        meios_warnings(ros_prefix_test)
        catch_discover_tests(ros_prefix_test TEST_PREFIX "ros_prefix.")
    endif()
endif()

if(TARGET meios_archive-zip)
    set(zip_writer_src ${CMAKE_CURRENT_SOURCE_DIR}/unit/zip_writer_test.cpp)
    if(EXISTS ${zip_writer_src})
        add_executable(zip_writer_test ${zip_writer_src})
        target_link_libraries(zip_writer_test
            PRIVATE meios::core meios::model meios::io meios::bundle meios::archive-zip
                miniz::miniz Catch2::Catch2WithMain)
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
    catch_discover_tests(eval_policy_test TEST_PREFIX "eval_policy.")
endif()
