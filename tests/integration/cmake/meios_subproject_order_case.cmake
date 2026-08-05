cmake_minimum_required(VERSION 3.28)

include("${CMAKE_CURRENT_LIST_DIR}/meios_cmake_content.cmake")

meios_harness_reset()

# Two trees rather than one reconfigured twice: the claim is over two orderings of the same
# listfile, and a second configure of one tree would carry the first ordering's cache into it.
foreach(_order IN ITEMS early late)
    if(NOT DEFINED _rc OR _rc EQUAL 0)
        meios_harness_configure("${FIXTURE}" "${WORK}/${_order}"
            "${EXTRA};-DFX_ORDER=${_order};-DFX_CONFIG_DUMP=${WORK}/${_order}.txt" _rc)
    endif()
endforeach()

if(_rc EQUAL 0)
    meios_harness_require_same_files("${WORK}/early.txt" "${WORK}/late.txt")
endif()

meios_harness_sentinel(${_rc})
