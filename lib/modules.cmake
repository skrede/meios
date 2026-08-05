# One helper expresses the whole module pattern. A module's public include root is
# lib/meios-<name>/include, so every target is consumed via #include <meios/...>;
# compiled modules also see lib/meios-<name>/src privately.

function(_meios_module_interface target include_root link_public)
    add_library(${target} INTERFACE)
    target_compile_features(${target} INTERFACE cxx_std_20)
    target_include_directories(${target} INTERFACE
        $<BUILD_INTERFACE:${include_root}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
    if(link_public)
        target_link_libraries(${target} INTERFACE ${link_public})
    endif()
endfunction()

function(_meios_module_compiled target type module_dir sources link_public link_private)
    add_library(${target} ${type} ${sources})
    meios_enable_coverage(${target})
    meios_warnings(${target})
    target_compile_features(${target} PUBLIC cxx_std_20)
    # A static meios archive is meant to embed inside a consumer's shared library; without PIC
    # that final link fails to relocate on Linux.
    set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_include_directories(${target}
        PUBLIC
            $<BUILD_INTERFACE:${module_dir}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        PRIVATE
            ${module_dir}/src)
    if(link_public)
        target_link_libraries(${target} PUBLIC ${link_public})
    endif()
    if(link_private)
        target_link_libraries(${target} PRIVATE ${link_private})
    endif()
endfunction()

# CMake aliases are not exported; EXPORT_NAME is what keeps the installed spelling (meios::core)
# identical to the build-tree alias.
function(_meios_module_publish target export_name fallback aliases)
    if(NOT export_name)
        set(export_name ${fallback})
    endif()
    set_target_properties(${target} PROPERTIES EXPORT_NAME ${export_name})
    foreach(alias IN LISTS aliases)
        add_library(${alias} ALIAS ${target})
    endforeach()
endfunction()

# The archive and the headers travel together, or neither travels: an installed header with no
# importable target behind it hands a consumer a translation unit that compiles and then fails to
# link, with nothing in the package explaining why. Withholding both makes the absence total, so
# the package's component mechanism is the single source of truth about it.
function(_meios_module_install target include_root reason)
    if(reason)
        message(STATUS "meios: ${target} builds but is not installed, because ${reason}")
        return()
    endif()
    install(TARGETS ${target}
        EXPORT meiosTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    if(EXISTS ${include_root})
        install(DIRECTORY ${include_root}/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    endif()
endfunction()

function(meios_add_module name)
    cmake_parse_arguments(ARG "" "TYPE;EXPORT_NAME;NOT_EXPORTABLE"
        "SOURCES;LINK_PUBLIC;LINK_PRIVATE;ALIASES" ${ARGN})
    set(target meios_${name})
    set(module_dir ${CMAKE_CURRENT_SOURCE_DIR}/meios-${name})
    # A reason rather than a flag, and an unrecorded reason is refused rather than merely noticed:
    # what the status line prints is why the module cannot be exported, and a future change
    # replaces that reason with a real dependency declaration.
    if("NOT_EXPORTABLE" IN_LIST ARGN AND NOT ARG_NOT_EXPORTABLE)
        message(FATAL_ERROR
            "meios: ${target} is marked NOT_EXPORTABLE with no reason. State why the module "
            "cannot join the installed export, or remove the marker.")
    endif()
    if(ARG_TYPE STREQUAL "INTERFACE")
        _meios_module_interface(${target} ${module_dir}/include "${ARG_LINK_PUBLIC}")
    else()
        _meios_module_compiled(${target} "${ARG_TYPE}" ${module_dir}
            "${ARG_SOURCES}" "${ARG_LINK_PUBLIC}" "${ARG_LINK_PRIVATE}")
    endif()
    _meios_module_publish(${target} "${ARG_EXPORT_NAME}" ${name} "${ARG_ALIASES}")
    if(MEIOS_INSTALL)
        _meios_module_install(${target} ${module_dir}/include "${ARG_NOT_EXPORTABLE}")
    endif()
endfunction()

# Opt-in enrichment modules. Each is its own compiled archive whose body lives under
# lib/meios-<name>/src, never meios-core/src, so the always-on core glob never absorbs an
# enrichment .cpp. A module target only materializes once its src carries a source; the glob guard
# keeps an options-on, sources-absent tree valid so an implementing plan adds only .cpp files.
function(meios_add_enrichment name)
    file(GLOB_RECURSE enrichment_sources CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/meios-${name}/src/*.cpp)
    if(NOT enrichment_sources)
        return()
    endif()
    meios_add_module(${name} TYPE STATIC ALIASES meios::${name}
        SOURCES ${enrichment_sources} ${ARGN})
endfunction()
