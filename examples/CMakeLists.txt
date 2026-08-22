# Each vendored description is copied beside the build tree and its path baked into
# a generated header, so an example never reaches into the source tree nor needs a
# path define injected at build time.

configure_file(robot.urdf.xacro ${CMAKE_CURRENT_BINARY_DIR}/robot.urdf.xacro COPYONLY)
configure_file(arg_robot.urdf.xacro ${CMAKE_CURRENT_BINARY_DIR}/arg_robot.urdf.xacro COPYONLY)
set(MEIOS_EXAMPLE_ROBOT "${CMAKE_CURRENT_BINARY_DIR}/robot.urdf.xacro")
set(MEIOS_EXAMPLE_ARG_ROBOT "${CMAKE_CURRENT_BINARY_DIR}/arg_robot.urdf.xacro")
configure_file(example_paths.h.in ${CMAKE_CURRENT_BINARY_DIR}/example_paths.h @ONLY)

# A group reads the generated header through this absolute path. Climbing to it from a child's
# own binary directory would bind the group to where the dispatcher happens to sit.
set(MEIOS_EXAMPLE_GENERATED_INCLUDE ${CMAKE_CURRENT_BINARY_DIR})

# Acquisition is declared here only because examples/ is the whole project; the split exists so a
# consumer can declare in its top-level listfile and deploy in the subdirectory owning the target.
#
# SOURCE_DIR mode carries the in-repo descriptions, and the pinned upstream comes through the same
# registry, so both land in one runtime directory — which is the layout package:// resolution wants,
# and what the plural RESOURCES argument is for. The upstream is fetched by default because the
# manifest example is only worth reading if it resolves a genuine package as well as an authored one;
# turning this off leaves the in-repo trees alone and needs no network.
option(MEIOS_EXAMPLE_FETCH_NETWORK "Fetch the pinned upstream description for the examples" ON)

meios_declare_resource(
    NAME example_arm
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/description)

meios_declare_resource(
    NAME example_ros_package
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ros_description)

# Two lists because the two examples want different trees: one demonstrates deployment and reads a
# description by path, the other crawls manifests and must see package directories as siblings.
set(MEIOS_EXAMPLE_RESOURCES example_arm)
set(MEIOS_EXAMPLE_ROS_RESOURCES example_ros_package)
if(MEIOS_EXAMPLE_FETCH_NETWORK)
    meios_declare_resource(
        NAME   example_kuka
        GITHUB ros-industrial/kuka_experimental
        REF    8d9292b04a22628b1b78d989e2ddd3abb913bf92
        HASH   SHA256=02f299d967868fb32022617429ddede3a197fb8f18e09b88f88732e11a78bf79)
    list(APPEND MEIOS_EXAMPLE_RESOURCES example_kuka)
    list(APPEND MEIOS_EXAMPLE_ROS_RESOURCES example_kuka)
endif()

# Every group listfile gates an enrichment example on the enrichment TARGET, not its option: an
# option can be ON with no materialized target (an empty source glob, or a missing optional
# dependency), so a target guard keeps an all-enrichments-OFF configure green.
add_subdirectory(bundling)
add_subdirectory(diagnostics)
add_subdirectory(loading)
add_subdirectory(resources)
add_subdirectory(scanners)
add_subdirectory(sources)
