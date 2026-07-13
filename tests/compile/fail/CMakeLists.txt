file(GLOB compile_fail_sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

foreach(source IN LISTS compile_fail_sources)
    get_filename_component(stem ${source} NAME_WE)
    add_library(${stem} OBJECT EXCLUDE_FROM_ALL ${source})
    target_link_libraries(${stem} PRIVATE meios::core meios::model)
    add_test(NAME compile_fail_${stem}
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
            --target ${stem} --config $<CONFIG>)
    set_tests_properties(compile_fail_${stem} PROPERTIES WILL_FAIL TRUE)
endforeach()
