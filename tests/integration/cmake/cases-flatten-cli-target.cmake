# The one case whose sub-configure names the binary as a target rather than as a path. Every other
# flatten case supplies MEIOS_CLI_EXECUTABLE, so the module's target branch — the generator
# expression standing in for the binary, and the target-level edge onto it — is reached nowhere
# else. The fixture builds its own stub binary, so the case registers whether or not this build
# produced a real one.
#
# Only the flatten rule is built, by name: that is what makes the ordering observable, because
# nothing else in the build selects the stub or the deployment.
meios_cmake_case(cmake_flatten_runs_a_cli_target
    FIXTURE flatten-cli-target
    DRIVER  meios_build_case.cmake
    ORIGIN  tarball
    EXTRA   -DFX_INPUT=pkg_a/urdf/robot.urdf.xacro
            -DFX_OUTPUT=models/robot.urdf
    BUILD_TARGET app_flatten_models_robot_urdf
    REQUIRE_PRESENT models/pkg_a/package.xml
    REQUIRE_CONTAINS models/robot.urdf,harness_stub_cli)
