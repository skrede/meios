# These cases configure and build a real binary target, which is why they use the build driver and
# the target fixture rather than the sub-second module fixture the declaration cases use.

meios_cmake_case(cmake_deploy_placement
    FIXTURE target
    DRIVER  meios_build_case.cmake
    ORIGIN  tarball
    EXTRA   -DFX_SUBDIR=models
    REQUIRE_PRESENT models/pkg_a/package.xml,models/pkg_b/config/params.yaml)

# A nested entry, not a flat one: a flat selection produces a deploy label with no separator in it,
# and the separator is what the stamp path has to survive.
meios_cmake_case(cmake_deploy_nested_selection
    FIXTURE target
    DRIVER  meios_build_case.cmake
    ORIGIN  tarball
    EXTRA   -DFX_SUBDIR=models -DFX_PACKAGES=pkg_b/config
    REQUIRE_PRESENT models/pkg_b/config/params.yaml)

# Every inter-word space is spelled as a character class because CMake re-wraps message(FATAL_ERROR)
# text at roughly 78 columns, so a pattern copied verbatim out of the module does not match what the
# case reads.
meios_cmake_case(cmake_deploy_containment_refusal
    FIXTURE target
    DRIVER  meios_build_case.cmake
    ORIGIN  tarball
    EXTRA   -DFX_PACKAGES=../escape
    REFUSES "must[ \t\r\n]+be[ \t\r\n]+a[ \t\r\n]+relative[ \t\r\n]+path[ \t\r\n]+inside[ \t\r\n]+the[ \t\r\n]+tree")
