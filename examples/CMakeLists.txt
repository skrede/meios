# Each vendored description is copied beside the build tree and its path baked into
# a generated header, so an example never reaches into the source tree nor needs a
# path define injected at build time.

add_executable(load_robot load_robot.cpp)
target_link_libraries(load_robot PRIVATE meios::core meios::model meios::urdf)

add_executable(xacro_overrides xacro_overrides.cpp)
target_link_libraries(xacro_overrides PRIVATE meios::core meios::model meios::urdf)

configure_file(robot.urdf.xacro ${CMAKE_CURRENT_BINARY_DIR}/robot.urdf.xacro COPYONLY)
configure_file(arg_robot.urdf.xacro ${CMAKE_CURRENT_BINARY_DIR}/arg_robot.urdf.xacro COPYONLY)
set(MEIOS_EXAMPLE_ROBOT "${CMAKE_CURRENT_BINARY_DIR}/robot.urdf.xacro")
set(MEIOS_EXAMPLE_ARG_ROBOT "${CMAKE_CURRENT_BINARY_DIR}/arg_robot.urdf.xacro")
configure_file(example_paths.h.in ${CMAKE_CURRENT_BINARY_DIR}/example_paths.h @ONLY)

target_include_directories(load_robot PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(xacro_overrides PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

meios_warnings(load_robot)
meios_warnings(xacro_overrides)

add_test(NAME example_load_robot COMMAND load_robot)
add_test(NAME example_xacro_overrides COMMAND xacro_overrides)

# Enrichment examples are gated on the enrichment TARGET, not its option: an option
# can be ON with no materialized target (an empty source glob, or a missing optional
# dependency), so a target guard keeps an all-enrichments-OFF configure green.
if(TARGET meios_scan-obj)
    add_executable(mesh_scan mesh_scan.cpp)
    target_link_libraries(mesh_scan PRIVATE meios::urdf meios::scan-obj)
    meios_warnings(mesh_scan)
    add_test(NAME example_mesh_scan COMMAND mesh_scan)
endif()

if(TARGET meios_ros)
    add_executable(ros_resolve ros_resolve.cpp)
    target_link_libraries(ros_resolve PRIVATE meios::urdf meios::ros)
    meios_warnings(ros_resolve)
    add_test(NAME example_ros_resolve COMMAND ros_resolve)
endif()

if(TARGET meios_archive-zip)
    add_executable(archive_bundle archive_bundle.cpp)
    target_link_libraries(archive_bundle PRIVATE meios::urdf meios::archive-zip)
    meios_warnings(archive_bundle)
    add_test(NAME example_archive_bundle COMMAND archive_bundle)
endif()

# Acquisition is declared here only because examples/ is the whole project; the split exists so a
# consumer can declare in its top-level listfile and deploy in the subdirectory owning the target.
#
# SOURCE_DIR mode carries the in-repo description so `ctest` never needs the network. The option
# adds a pinned upstream through the same registry, and both land in one runtime directory — which
# is the layout package:// resolution wants, and what the plural RESOURCES argument is for.
option(MEIOS_EXAMPLE_FETCH_NETWORK "Add a network-fetched description to the resource example" OFF)

meios_declare_resource(
    NAME example_arm
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/description)

set(MEIOS_EXAMPLE_RESOURCES example_arm)
if(MEIOS_EXAMPLE_FETCH_NETWORK)
    meios_declare_resource(
        NAME example_kuka
        URL  https://github.com/ros-industrial/kuka_experimental/archive/8d9292b04a22628b1b78d989e2ddd3abb913bf92.tar.gz
        HASH SHA256=02f299d967868fb32022617429ddede3a197fb8f18e09b88f88732e11a78bf79
        STRIP_TOP_LEVEL)
    list(APPEND MEIOS_EXAMPLE_RESOURCES example_kuka)
endif()

add_executable(deploy_resources deploy_resources.cpp)
target_link_libraries(deploy_resources PRIVATE meios::core meios::model meios::urdf)
meios_warnings(deploy_resources)
# No install form here on purpose: the examples are never install(TARGETS)-ed, so installing their
# resources would put description data in the install tree with no program to read it. A consumer
# that installs its executable wants INSTALL_RUNTIME_RELATIVE alongside this call.
meios_target_deploy_resources(deploy_resources
    RESOURCES ${MEIOS_EXAMPLE_RESOURCES}
    SUBDIR    urdf)
add_test(NAME example_deploy_resources COMMAND deploy_resources)
