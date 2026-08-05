cmake_minimum_required(VERSION 3.28)

# CXX because the module wiring declares libraries; every probe here is an INTERFACE module, so
# nothing is compiled and no case pays for a toolchain beyond its detection.
project(meios_cmake_module_export LANGUAGES CXX)

include(GNUInstallDirs)

# Included rather than copied, unlike the sibling fixture that plants an orphan: nothing here
# mutates the wiring, so the file under test is the file the build uses and no copy can drift.
include("${FX_MODULES_FILE}")

if(FX_WITHOUT_REASON STREQUAL "empty")
    meios_add_module(probe TYPE INTERFACE NOT_EXPORTABLE "")
elseif(FX_WITHOUT_REASON STREQUAL "absent")
    meios_add_module(probe TYPE INTERFACE NOT_EXPORTABLE)
endif()

# Two withheld modules and two kept ones, one of each on either side of the export install call.
# The kept pair is what makes the withheld pair's absence mean anything: if declaration position
# decided membership, kept-late would be missing from the installed export too.
if(FX_EXPORT_ORDER)
    set(MEIOS_INSTALL ON)
    meios_add_module(withheld-early TYPE INTERFACE NOT_EXPORTABLE "the early probe states a reason")
    meios_add_module(kept-early TYPE INTERFACE)
    install(EXPORT meiosTargets NAMESPACE meios:: FILE meiosTargets.cmake
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/meios)
    meios_add_module(withheld-late TYPE INTERFACE NOT_EXPORTABLE "the late probe states a reason")
    meios_add_module(kept-late TYPE INTERFACE)
endif()
