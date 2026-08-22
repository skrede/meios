include_guard(GLOBAL)

# The registry is a GLOBAL property rather than a PARENT_SCOPE variable so a resource declared in the
# top-level listfile is visible in a sibling subdirectory, which a PARENT_SCOPE variable never reaches.
function(_meios_register_resource name dir)
    get_property(_known GLOBAL PROPERTY MEIOS_RESOURCES)
    if("${name}" IN_LIST _known)
        get_property(_prev GLOBAL PROPERTY MEIOS_RESOURCE_${name}_DIR)
        if(NOT _prev STREQUAL "${dir}")
            message(FATAL_ERROR
                "meios_declare_resource(${name}): already declared with a different tree.\n"
                "  first: ${_prev}\n  now:   ${dir}")
        endif()
        return()
    endif()
    set_property(GLOBAL APPEND PROPERTY MEIOS_RESOURCES "${name}")
    set_property(GLOBAL PROPERTY MEIOS_RESOURCE_${name}_DIR "${dir}")
endfunction()

# Query a declared resource from any directory scope.
function(meios_resource_dir name out)
    get_property(_known GLOBAL PROPERTY MEIOS_RESOURCES)
    if(NOT "${name}" IN_LIST _known)
        string(REPLACE ";" ", " _list "${_known}")
        message(FATAL_ERROR
            "meios_resource_dir: no resource named '${name}'. Declared: ${_list}")
    endif()
    get_property(_dir GLOBAL PROPERTY MEIOS_RESOURCE_${name}_DIR)
    set(${out} "${_dir}" PARENT_SCOPE)
endfunction()
