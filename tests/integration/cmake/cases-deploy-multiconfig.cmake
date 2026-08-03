# The deployment follows the target's runtime directory, so under a generator carrying several
# configurations each one has to receive its own deployed tree. A rule gated on one stamp shared
# across configurations deploys into whichever configuration was built first and leaves the others
# empty, which no single-configuration case can see.
#
# The generator is not the one this build uses, so it has to be present independently. Its absence
# leaves the case unregistered rather than failing: the deployment is correct on a host that has
# only a single-configuration generator, it simply cannot be observed there.
execute_process(COMMAND "${CMAKE_COMMAND}" -E capabilities
                OUTPUT_VARIABLE _capabilities ERROR_QUIET)
if(_capabilities MATCHES "Ninja Multi-Config" AND CMAKE_MAKE_PROGRAM MATCHES "ninja")
    meios_cmake_case(cmake_deploy_follows_each_configuration
        FIXTURE target-multiconfig
        DRIVER  meios_multiconfig_case.cmake
        ORIGIN  tarball
        CONFIGS Debug,Release
        REQUIRE_PRESENT models/pkg_a/package.xml)
else()
    message(STATUS "meios-cmake: no multi-config generator; the per-configuration deploy case is not registered.")
endif()
