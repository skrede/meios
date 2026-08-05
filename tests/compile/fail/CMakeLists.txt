file(GLOB compile_fail_sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

# clang (incl. AppleClang) accepts designated-initializer reordering that gcc
# rejects, so these two negative-compile assertions are not portable: they would
# compile on a Clang frontend and violate WILL_FAIL. Register them only on a
# frontend that diagnoses the ordering; gcc keeps the coverage.
set(_designator_stems math_designator_order math_mixed_designator)

foreach(source IN LISTS compile_fail_sources)
    get_filename_component(stem ${source} NAME_WE)
    if(stem IN_LIST _designator_stems AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        continue()
    endif()
    add_library(${stem} OBJECT EXCLUDE_FROM_ALL ${source})
    target_link_libraries(${stem} PRIVATE
        meios::core meios::model meios::xacro meios::io meios::completion)
    # The top of the build tree is the only directory holding a generated build system; the
    # target and configuration arguments are what narrow the build down to this one probe.
    add_test(NAME compile_fail_${stem}
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
            --target ${stem} --config $<CONFIG>)
    set_tests_properties(compile_fail_${stem} PROPERTIES WILL_FAIL TRUE)
endforeach()
