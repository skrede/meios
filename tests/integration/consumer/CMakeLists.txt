cmake_minimum_required(VERSION 3.28)
project(meios_integration_test LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)

# One input picks the acquisition route and nothing else does. A declaration carrying only a
# source directory downloads nothing and ends in the same add_subdirectory a released consumer's
# declaration ends in, so the two routes differ in where meios came from and in nothing else. The
# working tree rather than a published revision is what a run before a push can validate.
if(MEIOS_CONSUMER_SOURCE_DIR)
    include(FetchContent)
    FetchContent_Declare(meios SOURCE_DIR "${MEIOS_CONSUMER_SOURCE_DIR}")
    FetchContent_MakeAvailable(meios)
else()
    find_package(meios CONFIG REQUIRED)
endif()

# The standard is deliberately not set on the target: it must arrive through the
# exported interface, proving the installed package carries it.
#
# Every source builds into the one executable rather than into further targets, so each
# exercise runs under the single command the continuous-integration jobs invoke.
add_executable(integration_test main.cpp facade.cpp scratch_probe.cpp asset_probe.cpp expand_probe.cpp)
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

include(${CMAKE_CURRENT_LIST_DIR}/universal_robots.cmake)
