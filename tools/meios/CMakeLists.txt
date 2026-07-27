include(GNUInstallDirs)

# The verb bodies are swept by a recursive glob so a new verb .cpp joins the support
# lib with no CMake edit; main.cpp is removed because it is the executable entry.
file(GLOB_RECURSE meios_cli_sources CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
list(REMOVE_ITEM meios_cli_sources ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)

add_library(meios_cli STATIC ${meios_cli_sources})
meios_enable_coverage(meios_cli)
meios_warnings(meios_cli)
target_compile_features(meios_cli PUBLIC cxx_std_20)
target_include_directories(meios_cli PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(meios_cli
    PUBLIC meios::urdf meios::bundle meios::completion
    PRIVATE CLI11::CLI11)

# The CLI is the consumer that links each built scanner target and calls its
# register_ free function. A per-scanner compile definition gates the matching block
# in register_scanners.cpp, so core and bundle stay free of any concrete scanner.
foreach(scan IN ITEMS obj collada stl gltf)
    if(TARGET meios_scan-${scan})
        string(TOUPPER ${scan} scan_upper)
        target_link_libraries(meios_cli PRIVATE meios::scan-${scan})
        target_compile_definitions(meios_cli PRIVATE MEIOS_CLI_HAS_SCAN_${scan_upper})
    endif()
endforeach()

# ROS package resolution composes as an ordinary source layer, gated the same way
# a scanner is: only when the enrichment target is built does the CLI link it and
# define the guard that folds a ros_package_source into build_sources.
if(TARGET meios_ros)
    target_link_libraries(meios_cli PRIVATE meios::ros)
    target_compile_definitions(meios_cli PRIVATE MEIOS_CLI_HAS_ROS)
endif()

# The Python evaluator backend links the same way an enrichment scanner does: only
# when the target is built does the CLI link it and define the guard that lets
# run_flatten construct a python-backed evaluator_handle for --eval python.
if(TARGET meios_eval-python)
    target_link_libraries(meios_cli PRIVATE meios::eval-python)
    target_compile_definitions(meios_cli PRIVATE MEIOS_CLI_HAS_EVAL_PYTHON)
    # A global property, not a scoped variable: a value set here would reach only the
    # parent directory, while the flatten call that reads it may be written in any
    # directory of a consumer's tree.
    set_property(GLOBAL PROPERTY MEIOS_CLI_HAS_EVAL_PYTHON TRUE)
endif()

add_executable(meios main.cpp)
meios_enable_coverage(meios)
meios_warnings(meios)
target_link_libraries(meios PRIVATE meios_cli)
set_target_properties(meios PROPERTIES OUTPUT_NAME meios EXPORT_NAME cli)
add_executable(meios::cli ALIAS meios)

if(MEIOS_ENABLE_INSTALL)
    install(TARGETS meios EXPORT meiosTargets RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
