# Three executables sharing a runtime directory deploy one tree into one place. A race cannot be
# proven absent by repetition, so the shared-directory cases are smoke tests; the held-lock case is
# what proves each step takes the lock.
meios_cmake_case(cmake_deploy_shared_directory
    FIXTURE shared-dir
    DRIVER  meios_shared_dir_case.cmake
    ORIGIN  tarball
    TIMEOUT 600
    REQUIRE_PRESENT models/pkg_a/package.xml,models/pkg_b/config/params.yaml)
set_tests_properties(cmake_deploy_shared_directory PROPERTIES PROCESSORS 3)

# The message is matched rather than refused because the case also has to see the build succeed
# once the lock is released. The pattern is one token of the timeout message that no command line
# carries, so a build tool's line prefixes and re-wrapping cannot split it.
meios_cmake_case(cmake_deploy_waits_for_a_held_lock
    FIXTURE shared-dir
    DRIVER  meios_lock_case.cmake
    ORIGIN  tarball
    MATCHES "MEIOS_STEP_LOCK_TIMEOUT,"
    REQUIRE_PRESENT models/pkg_a/package.xml)

meios_cmake_case(cmake_deploy_lock_timeout_refusal
    FIXTURE shared-dir
    ORIGIN  tarball
    EXTRA   -DMEIOS_STEP_LOCK_TIMEOUT=soon
    REFUSES "not[ \t\r\n]+a[ \t\r\n]+whole[ \t\r\n]+number[ \t\r\n]+of[ \t\r\n]+seconds")

if(TARGET meios)

set(_cli "-DMEIOS_CLI_EXECUTABLE=$<TARGET_FILE:meios>")

meios_cmake_case(cmake_flatten_shared_directory
    FIXTURE shared-dir
    DRIVER  meios_shared_dir_case.cmake
    ORIGIN  tarball
    TIMEOUT 600
    EXTRA   "${_cli}" -DFX_INPUT=pkg_a/urdf/robot.urdf.xacro -DFX_OUTPUT=urdf/robot.urdf
    REQUIRE_PRESENT models/pkg_a/package.xml
    REQUIRE_CONTAINS urdf/robot.urdf,harness_arm)
set_tests_properties(cmake_flatten_shared_directory PROPERTIES PROCESSORS 3)

# No deploy runs, so the step that waits on the lock can only be the flatten.
meios_cmake_case(cmake_flatten_waits_for_a_held_lock
    FIXTURE shared-dir
    DRIVER  meios_lock_case.cmake
    ORIGIN  tarball
    EXTRA   "${_cli}" -DFX_INPUT=pkg_a/urdf/robot.urdf.xacro -DFX_OUTPUT=urdf/robot.urdf
            -DFX_FLATTEN_ONLY=ON
    MATCHES "MEIOS_STEP_LOCK_TIMEOUT,"
    REQUIRE_CONTAINS urdf/robot.urdf,harness_arm)

endif()
