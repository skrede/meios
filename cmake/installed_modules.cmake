include_guard(GLOBAL)

# The lowercase name is deliberate: the installed modules are named Meios<Noun><Verb>.cmake, and
# the cross-check below globs exactly that pattern, so a build-time-only helper taking the
# lowercase form the other build-time helpers use is never a candidate for installation.

# The wiring's own include() lines are the list: an include that resolves is a file the installed
# package must carry. Those lines have to stay literal, because the modules also execute under a
# bare script run with no project and no variables, so a shared list variable could not be read by
# them; the derivation reads their include lines instead.
function(_meios_collect_included_modules dir entry out_var)
    set(_found "")
    set(_pending "${entry}")
    while(_pending)
        list(POP_FRONT _pending _file)
        if(_file IN_LIST _found)
            continue()
        endif()
        list(APPEND _found "${_file}")
        file(STRINGS "${dir}/${_file}" _lines
            REGEX "include\\(\"\\$\\{CMAKE_CURRENT_LIST_DIR\\}/")
        foreach(_line IN LISTS _lines)
            string(REGEX REPLACE "^.*CMAKE_CURRENT_LIST_DIR\\}/([^\"]+)\".*$" "\\1" _inc "${_line}")
            list(APPEND _pending "${_inc}")
        endforeach()
    endwhile()
    set(${out_var} "${_found}" PARENT_SCOPE)
endfunction()

# Reachable from no include(), because they are invoked as bare script runs rather than included,
# so they cannot be derived and have to be named.
set(_meios_cmake_script_modules MeiosPruneResources.cmake MeiosFlattenRun.cmake)

function(meios_installed_cmake_modules dir out_var)
    _meios_collect_included_modules("${dir}" MeiosDeclareResource.cmake _modules)
    file(GLOB _present RELATIVE "${dir}" "${dir}/Meios*.cmake")
    foreach(_file IN LISTS _present)
        if(NOT _file IN_LIST _modules AND NOT _file IN_LIST _meios_cmake_script_modules)
            message(FATAL_ERROR
                "meios: cmake/${_file} is reached by no include() and is not named as a script "
                "entry point, so the installed package would carry it only by accident. Include "
                "it from a module the wiring already reaches, name it beside the script entry "
                "points in installed_modules.cmake, or delete it.")
        endif()
    endforeach()
    list(APPEND _modules ${_meios_cmake_script_modules})
    list(TRANSFORM _modules PREPEND "${dir}/")
    set(${out_var} "${_modules}" PARENT_SCOPE)
endfunction()
