# Every case here drives the meios binary this build produces, so the whole file registers only
# where that binary exists and a tools-off configure lists no flatten case at all.
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

# The four configure-time refusals need no build, so they go through the default driver. Every
# pattern spells its inter-word spaces as a character class because CMake re-wraps message text at
# roughly 78 columns and breaks only at whitespace.
set(_declare -DFX_INPUT=pkg_a/urdf/robot.urdf.xacro -DFX_OUTPUT=urdf/robot.urdf)

# The fragment binds to the accepted values being listed, not merely to the value being rejected:
# a pass regex is one alternation, so it has to be the half that only appears when the list prints.
meios_cmake_case(cmake_flatten_eval_unknown_refusal
    FIXTURE flatten
    ORIGIN  tarball
    EXTRA   "${_cli}" ${_declare} -DFX_EVAL=pyhton
    REFUSES "Pass[ \t\r\n]+one[ \t\r\n]+of:[ \t\r\n]+core,[ \t\r\n]+python[.]")

# The capability is injected rather than read for the next two cases, because the logic has to be
# exercised in every job while the behavior can only be exercised where the enrichment was actually
# built, which is one job on one of the three platforms. Neither case builds, so a claim that does
# not match the binary can never become an execution attempt.
meios_cmake_case(cmake_flatten_eval_python_refusal
    FIXTURE flatten
    ORIGIN  tarball
    EXTRA   "${_cli}" ${_declare} -DFX_EVAL=python -DMEIOS_CLI_HAS_EVAL_PYTHON=FALSE
    REFUSES "Configure[ \t\r\n]+with[ \t\r\n]+MEIOS_BUILD_EVAL_PYTHON=ON")

meios_cmake_case(cmake_flatten_eval_python_statement
    FIXTURE flatten
    ORIGIN  tarball
    EXTRA   "${_cli}" ${_declare} -DFX_EVAL=python -DMEIOS_CLI_HAS_EVAL_PYTHON=TRUE
    MATCHES "with[ \t\r\n]+the[ \t\r\n]+python[ \t\r\n]+evaluator,[ \t\r\n]+which[ \t\r\n]+executes[ \t\r\n]+Python[ \t\r\n]+during[ \t\r\n]+the[ \t\r\n]+build")

# Nothing is injected: the sub-configure reaches the modules through the module path and so has no
# meios binary of any kind, which is the situation this refusal was written for.
meios_cmake_case(cmake_flatten_no_cli_refusal
    FIXTURE flatten
    ORIGIN  tarball
    EXTRA   ${_declare}
    REFUSES "Configure[ \t\r\n]+with[ \t\r\n]+MEIOS_BUILD_TOOLS=ON")

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
