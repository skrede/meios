# A standalone project, not part of the meios build. It configures against an *installed* meios and
# is the worked example for shipping a program together with the description it loads:
#
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=<meios install prefix>
#   cmake --build build
#   cmake --install build --prefix <somewhere>
#   <somewhere>/bin/install_consumer
#
# The last line is the point. The program resolves its description relative to its own executable,
# so the build tree and the install tree must lay the resource out the same way; deploying alone
# would leave the installed program with nothing to read.
cmake_minimum_required(VERSION 3.28)
project(meios_install_consumer LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)
find_package(meios CONFIG REQUIRED)

# SOURCE_DIR keeps this example offline. Swap in the pinned-archive form to stop vendoring the tree:
#   URL  https://github.com/<org>/<repo>/archive/<sha>.tar.gz
#   HASH SHA256=<hex>
#   STRIP_TOP_LEVEL
meios_declare_resource(
    NAME install_consumer_arm
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/description)

# The standard is deliberately not set on the target: it must arrive through the exported
# interface, proving the installed package carries it.
add_executable(install_consumer main.cpp)
target_link_libraries(install_consumer PRIVATE meios::urdf)

# SUBDIR places the tree at <runtime dir>/urdf in the build tree; INSTALL_RUNTIME_RELATIVE mirrors
# that under CMAKE_INSTALL_BINDIR so the same binary-relative lookup resolves after install.
meios_target_deploy_resources(install_consumer
    RESOURCES install_consumer_arm
    SUBDIR    urdf
    INSTALL_RUNTIME_RELATIVE)

install(TARGETS install_consumer RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

enable_testing()
add_test(NAME install_consumer COMMAND install_consumer)
