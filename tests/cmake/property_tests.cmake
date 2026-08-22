if(MEIOS_BUILD_PROPERTY_TESTS)
    set(xacro_eval_prop_src ${CMAKE_CURRENT_SOURCE_DIR}/property/xacro_eval_prop.cpp)
    if(EXISTS ${xacro_eval_prop_src})
        add_executable(xacro_eval_prop ${xacro_eval_prop_src})
        target_link_libraries(xacro_eval_prop
            PRIVATE meios::core meios::model meios::xacro
                rapidcheck rapidcheck_catch Catch2::Catch2WithMain)
        meios_enable_coverage(xacro_eval_prop)
        meios_warnings(xacro_eval_prop)
        catch_discover_tests(xacro_eval_prop)
    endif()

    set(urdf_prop_src ${CMAKE_CURRENT_SOURCE_DIR}/property/urdf_prop.cpp)
    if(EXISTS ${urdf_prop_src})
        add_executable(urdf_prop ${urdf_prop_src})
        target_link_libraries(urdf_prop
            PRIVATE meios::core meios::model meios::io meios::xacro meios::urdf
                rapidcheck rapidcheck_catch Catch2::Catch2WithMain)
        meios_enable_coverage(urdf_prop)
        meios_warnings(urdf_prop)
        catch_discover_tests(urdf_prop)
    endif()

    set(urdf_roundtrip_src ${CMAKE_CURRENT_SOURCE_DIR}/property/urdf_roundtrip_prop.cpp)
    if(EXISTS ${urdf_roundtrip_src})
        add_executable(urdf_roundtrip ${urdf_roundtrip_src})
        target_link_libraries(urdf_roundtrip
            PRIVATE meios::core meios::model meios::io meios::xacro meios::urdf
                meios::bundle rapidcheck rapidcheck_catch Catch2::Catch2WithMain)
        meios_enable_coverage(urdf_roundtrip)
        meios_warnings(urdf_roundtrip)
        catch_discover_tests(urdf_roundtrip)
    endif()
endif()
