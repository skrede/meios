add_executable(directory_source directory_source.cpp)
target_link_libraries(directory_source PRIVATE meios::core meios::model meios::io)
meios_warnings(directory_source)
add_test(NAME example_directory_source COMMAND directory_source)

add_executable(memory_source memory_source.cpp)
target_link_libraries(memory_source PRIVATE meios::core meios::model meios::io)
meios_warnings(memory_source)
add_test(NAME example_memory_source COMMAND memory_source)

add_executable(stacked_sources stacked_sources.cpp)
target_link_libraries(stacked_sources PRIVATE meios::core meios::model meios::io)
meios_warnings(stacked_sources)
add_test(NAME example_stacked_sources COMMAND stacked_sources)

if(TARGET meios_ros-package)
    add_executable(ros_resolve ros_resolve.cpp)
    target_link_libraries(ros_resolve PRIVATE meios::urdf meios::ros-package)
    meios_warnings(ros_resolve)
    meios_target_deploy_resources(ros_resolve
        RESOURCES ${MEIOS_EXAMPLE_ROS_RESOURCES}
        SUBDIR    ros_packages)
    # The crawl never examines its own search root, so each package has to arrive as a directory
    # beneath ros_packages/ rather than as its contents. Every reference is named on the command
    # line, which is what keeps the program's exit code tied to what was actually deployed.
    set(_ros_requests meios_example_description=meshes/link.stl)
    if(MEIOS_EXAMPLE_FETCH_NETWORK)
        list(APPEND _ros_requests kuka_lbr_iiwa_support=urdf/lbr_iiwa_14_r820.urdf)
    endif()
    add_test(NAME example_ros_resolve COMMAND ros_resolve ${_ros_requests})
endif()
