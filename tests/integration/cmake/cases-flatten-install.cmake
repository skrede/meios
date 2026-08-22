# The flatten cases that build and then install into a prefix of their own. They are what stands
# behind the claim that a flattened document installs under the rules the deploy call uses.
if(TARGET meios)

set(_cli "-DMEIOS_CLI_EXECUTABLE=$<TARGET_FILE:meios>")
set(_input -DFX_INPUT=pkg_a/urdf/robot.urdf.xacro)

# The document's OUTPUT sits inside the deployment root and both calls ask for the runtime-relative
# form, so what is asserted is that one destination rule serves both: the document has to arrive in
# the same installed directory as the tree it was expanded out of, not merely somewhere.
meios_cmake_case(cmake_flatten_install_runtime_relative
    FIXTURE flatten
    DRIVER  meios_build_case.cmake
    ORIGIN  tarball
    EXTRA   "${_cli}" ${_input} -DFX_OUTPUT=models/robot.urdf
            -DFX_INSTALL_RUNTIME_RELATIVE=ON
            -DFX_DEPLOY_INSTALL_RUNTIME_RELATIVE=ON
    REQUIRE_INSTALLED bin/models/robot.urdf,bin/models/pkg_a/package.xml)

# Exactly the named directory, with the directory part of OUTPUT not appended to it.
meios_cmake_case(cmake_flatten_install_destination
    FIXTURE flatten
    DRIVER  meios_build_case.cmake
    ORIGIN  tarball
    EXTRA   "${_cli}" ${_input} -DFX_OUTPUT=urdf/robot.urdf
            -DFX_INSTALL_DESTINATION=share/robot
    REQUIRE_INSTALLED share/robot/robot.urdf)

# The install run is restricted to the component the call named, so the document arrives only if its
# rule really carries that component: written into the default one it would install unfiltered and
# this run would place nothing at all.
meios_cmake_case(cmake_flatten_install_component
    FIXTURE flatten
    DRIVER  meios_build_case.cmake
    ORIGIN  tarball
    EXTRA   "${_cli}" ${_input} -DFX_OUTPUT=urdf/robot.urdf
            -DFX_INSTALL_DESTINATION=share/robot
            -DFX_INSTALL_COMPONENT=descriptions
    COMPONENT descriptions
    REQUIRE_INSTALLED share/robot/robot.urdf)

endif()
