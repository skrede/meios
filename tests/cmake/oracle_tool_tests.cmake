# The recorder's refusals are what keep a short or environment-degraded measurement out of the
# committed records, and a refusal is otherwise only ever taken on a broken run. Python is a found
# resource here as everywhere, so a tree without an interpreter registers none of these.
find_package(Python3 QUIET COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    foreach(oracle_tool_case IN ITEMS environment expressions manifests generated)
        add_test(NAME oracle_tools.${oracle_tool_case}
            COMMAND ${Python3_EXECUTABLE} -I -m unittest discover
                -s ${CMAKE_CURRENT_SOURCE_DIR}/tools/oracle -p test_${oracle_tool_case}.py)
    endforeach()
endif()
