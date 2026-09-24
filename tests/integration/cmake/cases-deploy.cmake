# These cases configure and build a real binary target, which is why they use the build driver and
# the target fixture rather than the sub-second module fixture the declaration cases use.

meios_cmake_case(cmake_deploy_placement
    FIXTURE target
    DRIVER  meios_build_case.cmake
    ORIGIN  tarball
    EXTRA   -DFX_SUBDIR=models
    REQUIRE_PRESENT models/pkg_a/package.xml,models/pkg_b/config/params.yaml)

# A nested entry rather than a flat one: only a nested entry has an intermediate directory to lose on
# the way to the destination.
meios_cmake_case(cmake_deploy_nested_selection
    FIXTURE target
    DRIVER  meios_build_case.cmake
    ORIGIN  tarball
    EXTRA   -DFX_SUBDIR=models -DFX_PACKAGES=pkg_b/config
    REQUIRE_PRESENT models/pkg_b/config/params.yaml)

# A package deployed under a directory named after itself carries its name into the rule twice
# unless the rule's name is bounded independently of both. The Windows leg runs this under the
# Visual Studio generator, where that length meets the path limit.
meios_cmake_case(cmake_deploy_bounded_rule_name
    FIXTURE target
    DRIVER  meios_build_case.cmake
    ORIGIN  tarball
    EXTRA   -DFX_NAME=harness_description_package
            -DFX_SUBDIR=urdf/harness_description_package
            -DFX_PACKAGES=pkg_b/config
    REQUIRE_PRESENT urdf/harness_description_package/pkg_b/config/params.yaml)

# The two SUBDIRs are the pair a character-substituting name folds together. A successful configure
# is itself the assertion, since two equal rule names are a duplicate target.
meios_cmake_case(cmake_deploy_distinct_rule_names
    FIXTURE target
    DRIVER  meios_build_case.cmake
    ORIGIN  tarball
    EXTRA   -DFX_SUBDIR=models/pkg -DFX_SECOND_SUBDIR=models_pkg
    REQUIRE_PRESENT models/pkg/pkg_a/package.xml,models_pkg/pkg_a/package.xml)

# The edit touches no source file, so nothing relinks: a deployment attached as a post-build command
# would leave the stale tree in place with no sign anything was wrong.
meios_cmake_case(cmake_deploy_reruns_on_edit
    FIXTURE target
    DRIVER  meios_build_case.cmake
    ORIGIN  tarball
    EXTRA   -DFX_SUBDIR=models
    MUTATE  pkg_a/package.xml
    REQUIRE_CONTAINS models/pkg_a/package.xml,meios-harness-mutated)

# The sibling above only proves an edit stamped forward in time redeploys, which a rule comparing
# modification times also gets right. Back-dating the edit is what separates the two: a stamped rule
# sees an input older than its own stamp and skips, while a content comparison republishes.
meios_cmake_case(cmake_deploy_reruns_on_backdated_edit
    FIXTURE target
    DRIVER  meios_build_case.cmake
    ORIGIN  tarball
    EXTRA   -DFX_SUBDIR=models
    MUTATE  pkg_a/package.xml
    MUTATE_BACKDATE ON
    REQUIRE_CONTAINS models/pkg_a/package.xml,meios-harness-mutated)

# Every inter-word space is spelled as a character class because CMake re-wraps message(FATAL_ERROR)
# text at roughly 78 columns, so a pattern copied verbatim out of the module does not match what the
# case reads.
meios_cmake_case(cmake_deploy_containment_refusal
    FIXTURE target
    DRIVER  meios_build_case.cmake
    ORIGIN  tarball
    EXTRA   -DFX_PACKAGES=../escape
    REFUSES "must[ \t\r\n]+be[ \t\r\n]+a[ \t\r\n]+relative[ \t\r\n]+path[ \t\r\n]+inside[ \t\r\n]+the[ \t\r\n]+tree")
