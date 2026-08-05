cmake_minimum_required(VERSION 3.28)

# NONE for the reason the sibling fixture takes it: the derivation and its cross-check both
# complete at configure time and need no toolchain.
project(meios_cmake_modules_fixture NONE)

# A copy, because a case that introduces a module reached by nobody must not edit the module
# directory the rest of the suite is reading at the same time.
file(COPY "${FX_MODULES_FROM}/" DESTINATION "${PROJECT_BINARY_DIR}/modules")
if(FX_ORPHAN)
    file(WRITE "${PROJECT_BINARY_DIR}/modules/${FX_ORPHAN}" "")
endif()

include("${PROJECT_BINARY_DIR}/modules/installed_modules.cmake")
meios_installed_cmake_modules("${PROJECT_BINARY_DIR}/modules" _modules)

list(LENGTH _modules _count)
message(STATUS "meios-fixture: derived ${_count} module(s)")
