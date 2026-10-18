# The read-back refuses only on a broken install, so CI never sees most of its refusals taken. Each
# is driven here over a scratch prefix and tree spoiled in that one way; the clean pair is the
# control. The read-back's Windows legs read PE images and are not reproduced here.
find_program(MEIOS_BASH_EXECUTABLE bash)
if(MEIOS_BASH_EXECUTABLE AND NOT WIN32)
    foreach(read_back_case IN ITEMS clean header_names_dependency header_names_namespace
                                    headers_unsearchable headers_missing tree_missing probe_missing
                                    probe_not_an_object core_missing core_references_dependency)
        add_test(NAME read_back_control.${read_back_case}
            COMMAND ${MEIOS_BASH_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/read_back_controls.sh
                ${meios_SOURCE_DIR}/.github/scripts/read_back_consumer.sh ${read_back_case})
        set_tests_properties(read_back_control.${read_back_case} PROPERTIES
            PASS_REGULAR_EXPRESSION "PASS: ${read_back_case}"
            SKIP_REGULAR_EXPRESSION "SKIP: "
            FAIL_REGULAR_EXPRESSION "FAIL: ")
    endforeach()
endif()
