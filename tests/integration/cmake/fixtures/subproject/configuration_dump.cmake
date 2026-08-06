# A consumer target declared on either side of the add_subdirectory, and a read-back of what meios
# configured, so that two trees differing only in that position can be compared line for line.

function(fx_declare_consumer_interface)
    add_library(fx_consumer_interface INTERFACE)
    target_compile_definitions(fx_consumer_interface INTERFACE FX_CONSUMER=1)
    target_include_directories(fx_consumer_interface INTERFACE "${PROJECT_SOURCE_DIR}")
endfunction()

function(_fx_describe_target target out)
    set(_text "")
    foreach(_property IN ITEMS TYPE INCLUDE_DIRECTORIES INTERFACE_INCLUDE_DIRECTORIES
                               INTERFACE_COMPILE_DEFINITIONS INTERFACE_COMPILE_FEATURES
                               INTERFACE_LINK_LIBRARIES EXPORT_NAME)
        get_target_property(_value ${target} ${_property})
        string(APPEND _text "${target} ${_property} = ${_value}\n")
    endforeach()
    set(${out} "${_text}" PARENT_SCOPE)
endfunction()

# The binary directory is folded to a token because the two trees being compared are two directories
# by construction, and every path meios records that names one would differ for that reason alone.
function(fx_write_configuration path)
    set(_dump "")
    foreach(_name IN ITEMS core model io urdf xacro bundle completion scan-obj scan-collada
                           scan-stl scan-gltf archive-zip eval-python ros-package cli)
        if(NOT TARGET meios::${_name})
            string(APPEND _dump "meios::${_name} absent\n")
            continue()
        endif()
        _fx_describe_target(meios::${_name} _text)
        string(APPEND _dump "${_text}")
    endforeach()
    string(REPLACE "${PROJECT_BINARY_DIR}" "<tree>" _dump "${_dump}")
    file(WRITE "${path}" "${_dump}")
endfunction()
