cmake_minimum_required(VERSION 3.28)

# NONE is deliberate: acquisition, containment, claims and refusal all complete before generation
# and need no toolchain, which removes the whole compiler-passthrough surface and keeps a case well
# under a second.
project(meios_cmake_fixture NONE)

include(MeiosDeclareResource)

set(_args NAME "${FX_NAME}")
foreach(_keyword IN ITEMS URL HASH GIT_REPOSITORY GIT_TAG SOURCE_DIR SUBDIR)
    if(FX_${_keyword})
        list(APPEND _args ${_keyword} "${FX_${_keyword}}")
    endif()
endforeach()
if(FX_SPARSE_PATHS)
    list(APPEND _args SPARSE_PATHS ${FX_SPARSE_PATHS})
endif()
if(FX_STRIP_TOP_LEVEL)
    list(APPEND _args STRIP_TOP_LEVEL)
endif()

meios_declare_resource(${_args} OUT_DIR _tree)

# ',' separates a multi-entry assertion because ';' is already spoken for: it is what separates the
# elements the case hands the driver through a single -D.
string(REPLACE "," ";" _require_dirs   "${FX_REQUIRE_DIR}")
string(REPLACE "," ";" _require_absent "${FX_REQUIRE_ABSENT}")

foreach(_rel IN LISTS _require_dirs)
    if(NOT IS_DIRECTORY "${_tree}/${_rel}")
        message(FATAL_ERROR "meios-fixture: '${_rel}' is not a directory under ${_tree}")
    endif()
endforeach()
foreach(_rel IN LISTS _require_absent)
    if(EXISTS "${_tree}/${_rel}")
        message(FATAL_ERROR "meios-fixture: '${_rel}' is present under ${_tree} and should not be")
    endif()
endforeach()
if(FX_MARK)
    file(WRITE "${_tree}/${FX_MARK}" "")
endif()
