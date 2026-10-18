# A native load leg handed only some of its inputs must refuse at configure, because a leg that
# configures without them is a leg that silently never runs. The two supplied inputs name paths
# that exist, so the only thing left to refuse is the omission. The XML parser is pointed at the
# copy this tree already holds when it fetched one, so the case reaches the guard without a download.
set(_inputs_args
    "-DMEIOS_CONSUMER_SOURCE_DIR=${meios_SOURCE_DIR}"
    -DMEIOS_YAML_SUPPORT=OFF -DMEIOS_ROS_PACKAGE_SUPPORT=OFF -DMEIOS_BUILD_TESTS=OFF
    "-DMEIOS_CONSUMER_UR3E_DOCUMENT=${meios_SOURCE_DIR}/README.md"
    "-DMEIOS_CONSUMER_UR3E_PACKAGE_ROOT=${meios_SOURCE_DIR}")
if(pugixml_SOURCE_DIR)
    list(APPEND _inputs_args "-DFETCHCONTENT_SOURCE_DIR_PUGIXML=${pugixml_SOURCE_DIR}")
endif()

meios_cmake_case(cmake_consumer_partial_ur3e_inputs_refusal
    DRIVER  meios_consumer_inputs_case.cmake
    EXTRA   ${_inputs_args}
    REFUSES "without[ \t\r\n]+MEIOS_CONSUMER_TESTS_DIR\\.[ \t\r\n]+A[ \t\r\n]+partially[ \t\r\n]+specified")
