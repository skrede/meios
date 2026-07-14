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

add_executable(meios main.cpp)
meios_enable_coverage(meios)
meios_warnings(meios)
target_link_libraries(meios PRIVATE meios_cli)
set_target_properties(meios PROPERTIES OUTPUT_NAME meios)

if(MEIOS_ENABLE_INSTALL)
    install(TARGETS meios RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
