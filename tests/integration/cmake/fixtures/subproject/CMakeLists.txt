cmake_minimum_required(VERSION 3.28)

# CXX rather than NONE: meios declares CXX itself, and this project compiles a consumer translation
# unit against meios's public headers. The project() call here is also the condition under test --
# it retargets PROJECT_SOURCE_DIR, so meios's own listfiles have to name their own project variable.
project(meios_cmake_subproject LANGUAGES CXX)

# FetchContent_MakeAvailable ends in add_subdirectory, so the semantics are identical, and the
# working tree rather than a committed revision is what a run before a push has to validate.
add_subdirectory("${FX_MEIOS_SOURCE_DIR}" meios-build)

# An include root that does not exist is a green configure and a doomed compile, and building the
# core archive plus a fetched CLI11 inside every case is not affordable, so the roots are read back
# instead. A generator expression is skipped: it resolves after this point.
foreach(_target IN ITEMS meios_cli meios_core)
    if(NOT TARGET ${_target})
        continue()
    endif()
    get_target_property(_roots ${_target} INCLUDE_DIRECTORIES)
    foreach(_root IN LISTS _roots)
        if(_root MATCHES "\\$<")
            continue()
        endif()
        if(NOT IS_DIRECTORY "${_root}")
            message(FATAL_ERROR
                "meios-fixture: ${_target} attaches '${_root}', which is not a directory")
        endif()
    endforeach()
endforeach()

add_executable(meios_subproject_consumer consumer.cpp)
target_link_libraries(meios_subproject_consumer PRIVATE meios::urdf)
