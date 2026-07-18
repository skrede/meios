cmake_minimum_required(VERSION 3.28)
project(meios_integration_test LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)

find_package(meios CONFIG REQUIRED)

# The standard is deliberately not set on the target: it must arrive through the
# exported interface, proving the installed package carries it.
add_executable(integration_test main.cpp)
target_link_libraries(integration_test PRIVATE meios::core meios::model meios::urdf)

enable_testing()
add_test(NAME integration COMMAND integration_test)
