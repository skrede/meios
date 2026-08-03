# The cases here build. Their configure-only siblings are in cases-flatten-configure.cmake and the
# ones that also install are in cases-flatten-install.cmake. Every one drives the meios binary this
# build produces, so each file registers only where that binary exists and a tools-off configure
# lists no flatten case at all.
if(TARGET meios)

# The sub-configure reaches the modules through CMAKE_MODULE_PATH and this repository publishes no
# build-tree package, so it has no meios::cli to import; the caller-supplied path is the seam that
# exists for exactly that, and is the same one a cross-compiling build uses. The consequence is that
# every case below takes either the first branch of the selection order or the last, leaving the two
# middle ones (an imported or aliased exported target, and a plain in-tree target) unexercised.
set(_cli "-DMEIOS_CLI_EXECUTABLE=$<TARGET_FILE:meios>")

meios_cmake_case(cmake_flatten_parameterized
    FIXTURE flatten
    DRIVER  meios_build_case.cmake
    ORIGIN  tarball
    EXTRA   "${_cli}"
            -DFX_INPUT=pkg_a/urdf/robot.urdf.xacro
            -DFX_OUTPUT=urdf/robot.urdf
            -DFX_ARGS=prefix:=zz_
    REQUIRE_CONTAINS urdf/robot.urdf,zz_upper)

# The pattern is a fragment of the binary's own typed diagnostic rather than the build tool's
# failure line, so the case fails for the reason it names: a missing input would also return
# non-zero. The absent paths are the other half of it: a rule writing straight into its output would
# leave a fragment of a URDF behind, which parses far enough to look like one.
meios_cmake_case(cmake_flatten_failure_diagnostic
    FIXTURE flatten
    DRIVER  meios_build_case.cmake
    ORIGIN  tarball
    EXTRA   "${_cli}"
            -DFX_INPUT=pkg_a/urdf/broken.urdf.xacro
            -DFX_OUTPUT=urdf/broken.urdf
    REFUSES "broken[.]urdf[.]xacro:[0-9]+:[0-9]+:[ \t\r\n]+\\[error\\][ \t\r\n]+\\(undefined_property\\)"
    REQUIRE_ABSENT urdf/broken.urdf,urdf/broken.urdf.tmp)

# The edit replaces the description the rule reads and touches no source file, so nothing relinks:
# a rule attached to the target's link step would leave the stale document in place with no sign
# anything was wrong. The expected name exists nowhere in the acquired tree, so a document that was
# not regenerated cannot carry it.
meios_cmake_case(cmake_flatten_reruns_on_edit
    FIXTURE flatten
    DRIVER  meios_build_case.cmake
    ORIGIN  tarball
    EXTRA   "${_cli}"
            -DFX_INPUT=pkg_a/urdf/robot.urdf.xacro
            -DFX_OUTPUT=urdf/robot.urdf
    MUTATE      pkg_a/urdf/robot.urdf.xacro
    MUTATE_FROM mutations/robot.urdf.xacro
    REQUIRE_CONTAINS urdf/robot.urdf,meios_harness_mutated)

# Only the flatten rule is built, which is what makes the ordering observable at all: the document
# lands inside the deployment root and the wrapper creates its own parent, so a rule not depending
# on the deploy target would produce the document into an otherwise empty directory. Naming the rule
# is the point of the case: nothing else selects it on its own.
meios_cmake_case(cmake_flatten_orders_after_deploy
    FIXTURE flatten
    DRIVER  meios_build_case.cmake
    ORIGIN  tarball
    EXTRA   "${_cli}"
            -DFX_INPUT=pkg_a/urdf/robot.urdf.xacro
            -DFX_OUTPUT=models/robot.urdf
    BUILD_TARGET app_flatten_models_robot_urdf
    REQUIRE_PRESENT models/pkg_a/package.xml,models/pkg_b/config/params.yaml
    REQUIRE_CONTAINS models/robot.urdf,harness_arm)

# What makes the entry load-bearing is the crawl stopping at a manifest: pkg_inner sits under
# pkg_outer's directory, so the acquired tree as the sole root reaches pkg_outer and stops, and the
# include fails to resolve. The vendored directory is named inner_dir rather than pkg_inner so that
# joining the reference onto the entry cannot resolve it either — only the manifest read under the
# added root can — and the expected link exists in no other package in the tree.
#
# That manifest read is what meios::ros carries, so the case registers only where the binary was
# really built with it. The enrichment is on by default, so this excuses the case in a deliberately
# minimal build rather than in an ordinary one.
get_property(_cli_has_ros GLOBAL PROPERTY MEIOS_CLI_HAS_ROS)
if(_cli_has_ros)
    meios_cmake_case(cmake_flatten_package_path_reaches_vendored_package
        FIXTURE flatten
        DRIVER  meios_build_case.cmake
        ORIGIN  tarball
        EXTRA   "${_cli}"
                -DFX_INPUT=pkg_outer/urdf/nested.urdf.xacro
                -DFX_OUTPUT=urdf/nested.urdf
                -DFX_PACKAGE_PATH=pkg_outer/vendor
        REQUIRE_CONTAINS urdf/nested.urdf,harness_vendored_link)
endif()

# The one case that executes Python, so it registers only where the binary was really built with the
# enrichment. What it injects is the value of the property the binary's own listfile recorded, not a
# claim about it: a sub-configure is a separate process with no meios package to look up, so a global
# property cannot reach it any other way. Its description carries an expression the core evaluator
# refuses by name, so the expected document is one only the python backend can have produced and a
# silent downgrade could not pass this case.
get_property(_cli_has_python GLOBAL PROPERTY MEIOS_CLI_HAS_EVAL_PYTHON)
if(_cli_has_python)
    meios_cmake_case(cmake_flatten_eval_python_live
        FIXTURE flatten
        DRIVER  meios_build_case.cmake
        ORIGIN  tarball
        EXTRA   "${_cli}" -DFX_INPUT=pkg_a/urdf/python.urdf.xacro -DFX_OUTPUT=urdf/python.urdf
                -DFX_EVAL=python -DFX_ARGS=prefix:=py_
                "-DMEIOS_CLI_HAS_EVAL_PYTHON=${_cli_has_python}"
        REQUIRE_CONTAINS urdf/python.urdf,py_upper)
endif()

endif()
