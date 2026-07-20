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
