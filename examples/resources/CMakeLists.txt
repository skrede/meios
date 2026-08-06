add_executable(deploy_resources deploy_resources.cpp)
target_link_libraries(deploy_resources PRIVATE meios::core meios::model meios::urdf)
meios_warnings(deploy_resources)
# No install form here on purpose: the examples are never install(TARGETS)-ed, so installing their
# resources would put description data in the install tree with no program to read it. A consumer
# that installs its executable wants INSTALL_RUNTIME_RELATIVE alongside this call.
meios_target_deploy_resources(deploy_resources
    RESOURCES ${MEIOS_EXAMPLE_RESOURCES}
    SUBDIR    urdf)
add_test(NAME example_deploy_resources COMMAND deploy_resources)
