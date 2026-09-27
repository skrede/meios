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

# A package deployed under a directory named after itself carries that name twice into any name built
# from the call's arguments, while the step is named after the target alone. The Windows leg runs
# this under the Visual Studio generator, which names a rule file after that step.
meios_cmake_case(cmake_deploy_bounded_step_name
    FIXTURE target
    DRIVER  meios_build_case.cmake
    ORIGIN  tarball
    EXTRA   -DFX_NAME=harness_description_package
            -DFX_SUBDIR=urdf/harness_description_package
            -DFX_PACKAGES=pkg_b/config
    REQUIRE_PRESENT urdf/harness_description_package/pkg_b/config/params.yaml)

# Two deploy calls on one target: the second appends to the step the first created, and each tree
# lands in its own SUBDIR, even where the two SUBDIRs differ only by `/` against `_`.
meios_cmake_case(cmake_deploy_two_calls_one_step
    FIXTURE target
    DRIVER  meios_build_case.cmake
    ORIGIN  tarball
    EXTRA   -DFX_SUBDIR=models/pkg -DFX_SECOND_SUBDIR=models_pkg
    REQUIRE_PRESENT models/pkg/pkg_a/package.xml,models_pkg/pkg_a/package.xml)

# Two entries out of one tree are deployed through the one step the target carries.
meios_cmake_case(cmake_deploy_one_step_two_packages
    FIXTURE target
    DRIVER  meios_build_case.cmake
    ORIGIN  tarball
    EXTRA   -DFX_SUBDIR=models -DFX_PACKAGES=pkg_a,pkg_b/config
    REQUIRE_PRESENT models/pkg_a/package.xml,models/pkg_b/config/params.yaml)

# A call naming two resources deploys both through the one step the target carries.
meios_cmake_case(cmake_deploy_one_step_two_resources
    FIXTURE target
    DRIVER  meios_build_case.cmake
    ORIGIN  tarball
    EXTRA   -DFX_SUBDIR=models -DFX_SECOND_RESOURCE=demo_b
    REQUIRE_PRESENT models/pkg_a/package.xml,models/config/params.yaml)

# CMake's if() reads a lone `off` as false, so a selection tested for truth rather than emptiness
# deploys the whole tree instead of the one entry named.
meios_cmake_case(cmake_deploy_false_constant_package
    FIXTURE target
    DRIVER  meios_build_case.cmake
    ORIGIN  tarball
    EXTRA   -DFX_SUBDIR=models -DFX_PACKAGES=off
    REQUIRE_PRESENT models/off/package.xml
    REQUIRE_ABSENT  models/pkg_a/package.xml)

# The edit touches no source file, so nothing relinks: a deployment attached as a post-build command
# would leave the stale tree in place with no sign anything was wrong.
meios_cmake_case(cmake_deploy_reruns_on_edit
    FIXTURE target
    DRIVER  meios_build_case.cmake
    ORIGIN  tarball
    EXTRA   -DFX_SUBDIR=models
    MUTATE  pkg_a/package.xml
    REQUIRE_CONTAINS models/pkg_a/package.xml,meios-harness-mutated)

# The sibling above only proves an edit stamped forward in time redeploys, which a step comparing
# modification times also gets right. Back-dating the edit is what separates the two: a stamped step
# sees an input older than its own stamp and skips, while a content comparison republishes.
meios_cmake_case(cmake_deploy_reruns_on_backdated_edit
    FIXTURE target
    DRIVER  meios_build_case.cmake
    ORIGIN  tarball
    EXTRA   -DFX_SUBDIR=models
    MUTATE  pkg_a/package.xml
    MUTATE_BACKDATE ON
    REQUIRE_CONTAINS models/pkg_a/package.xml,meios-harness-mutated)

# The edit redeploys in a build that must not relink, which is what tells a step attached as a source
# apart from one attached to the link.
meios_cmake_case(cmake_deploy_does_not_relink
    FIXTURE target
    DRIVER  meios_build_case.cmake
    ORIGIN  tarball
    EXTRA   -DFX_SUBDIR=models
    MUTATE  pkg_a/package.xml
    REQUIRE_CONTAINS models/pkg_a/package.xml,meios-harness-mutated
    REQUIRE_UNRELINKED app)

# Every inter-word space is spelled as a character class because CMake re-wraps message(FATAL_ERROR)
# text at roughly 78 columns, so a pattern copied verbatim out of the module does not match what the
# case reads.
meios_cmake_case(cmake_deploy_containment_refusal
    FIXTURE target
    DRIVER  meios_build_case.cmake
    ORIGIN  tarball
    EXTRA   -DFX_PACKAGES=../escape
    REFUSES "must[ \t\r\n]+be[ \t\r\n]+a[ \t\r\n]+relative[ \t\r\n]+path[ \t\r\n]+inside[ \t\r\n]+the[ \t\r\n]+tree")

# A step created from another directory configures, builds, and never runs, so without the refusal a
# tree would silently never deploy.
meios_cmake_case(cmake_deploy_other_directory_refusal
    FIXTURE target
    DRIVER  meios_build_case.cmake
    ORIGIN  tarball
    EXTRA   -DFX_ELSEWHERE=ON
    REFUSES "must[ \t\r\n]+be[ \t\r\n]+called[ \t\r\n]+in[ \t\r\n]+the[ \t\r\n]+directory[ \t\r\n]+that[ \t\r\n]+defines")
