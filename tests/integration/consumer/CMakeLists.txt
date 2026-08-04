cmake_minimum_required(VERSION 3.28)
project(meios_integration_test LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)

find_package(meios CONFIG REQUIRED)

# The standard is deliberately not set on the target: it must arrive through the
# exported interface, proving the installed package carries it.
#
# Every source builds into the one executable rather than into further targets, so each
# exercise runs under the single command the continuous-integration jobs invoke.
add_executable(integration_test main.cpp facade.cpp scratch_probe.cpp asset_probe.cpp)
target_link_libraries(integration_test PRIVATE meios::urdf meios::bundle)

# The consumer derives the vendored fixture's location itself: the xacro is copied
# beside the build tree and its path baked into a generated header, so the test never
# needs an external path define nor a reach into a parent source tree.
configure_file(robot.urdf.xacro ${CMAKE_CURRENT_BINARY_DIR}/robot.urdf.xacro COPYONLY)
set(MEIOS_CONSUMER_FIXTURE "${CMAKE_CURRENT_BINARY_DIR}/robot.urdf.xacro")
configure_file(fixture_path.h.in ${CMAKE_CURRENT_BINARY_DIR}/fixture_path.h @ONLY)
target_include_directories(integration_test PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

enable_testing()
add_test(NAME integration COMMAND integration_test)
