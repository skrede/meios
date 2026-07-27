file(GLOB compile_pass_sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

# Unlike the negative assertions next door these belong in ALL: a claim that the public
# headers still admit a shape is only worth making if breaking it turns the build red.
# The ctest entry exists so the same claim is selectable by regex alongside them.
foreach(source IN LISTS compile_pass_sources)
    get_filename_component(stem ${source} NAME_WE)
    add_library(${stem} OBJECT ${source})
    target_link_libraries(${stem} PRIVATE
        meios::core meios::model meios::xacro meios::io meios::urdf)
    meios_warnings(${stem})
    add_test(NAME compile_pass_${stem}
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
            --target ${stem} --config $<CONFIG>)
endforeach()
