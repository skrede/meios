file(GLOB fuzz_sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*_fuzz.cpp)

foreach(source IN LISTS fuzz_sources)
    get_filename_component(stem ${source} NAME_WE)
    add_executable(${stem} ${source})
    target_compile_options(${stem} PRIVATE -fsanitize=fuzzer,address)
    target_link_options(${stem} PRIVATE -fsanitize=fuzzer,address)
    target_link_libraries(${stem}
        PRIVATE meios::core meios::xacro meios::io meios::model meios::urdf)
endforeach()
