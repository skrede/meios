# The flatten cases that stop at configure. They need no build, so they go through the default
# driver. Every pattern spells its inter-word spaces as a character class because CMake re-wraps
# message text at roughly 78 columns and breaks only at whitespace.
if(TARGET meios)

set(_cli "-DMEIOS_CLI_EXECUTABLE=$<TARGET_FILE:meios>")
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

meios_cmake_case(cmake_flatten_no_input_refusal
    FIXTURE flatten
    ORIGIN  tarball
    EXTRA   "${_cli}" -DFX_OUTPUT=urdf/robot.urdf
    REFUSES "INPUT[ \t\r\n]+names[ \t\r\n]+the[ \t\r\n]+description[ \t\r\n]+to[ \t\r\n]+expand")

# A path that names nothing is refused at configure with the full path it looked at, rather than
# reaching the binary as a build-time failure.
meios_cmake_case(cmake_flatten_unknown_input_refusal
    FIXTURE flatten
    ORIGIN  tarball
    EXTRA   "${_cli}" -DFX_INPUT=pkg_a/urdf/absent.urdf.xacro -DFX_OUTPUT=urdf/robot.urdf
    REFUSES "names[ \t\r\n]+no[ \t\r\n]+file[ \t\r\n]+in[ \t\r\n]+resource")

# Each containment pattern names its own keyword, because one shared message serves all three and a
# pattern binding only to the shared half would pass whichever keyword had been checked. INPUT is
# checked before the path is looked for on disk, and that order is what the case pins: with the
# check gone the value reaches the existence test instead, which refuses under different text.
meios_cmake_case(cmake_flatten_input_containment_refusal
    FIXTURE flatten
    ORIGIN  tarball
    EXTRA   "${_cli}" -DFX_INPUT=../escape/robot.urdf.xacro -DFX_OUTPUT=urdf/robot.urdf
    REFUSES "INPUT[ \t\r\n]+entry[ \t\r\n]+'[.][.]/escape/robot[.]urdf[.]xacro'")

meios_cmake_case(cmake_flatten_output_containment_refusal
    FIXTURE flatten
    ORIGIN  tarball
    EXTRA   "${_cli}" -DFX_INPUT=pkg_a/urdf/robot.urdf.xacro -DFX_OUTPUT=../escape/robot.urdf
    REFUSES "OUTPUT[ \t\r\n]+entry[ \t\r\n]+'[.][.]/escape/robot[.]urdf'")

meios_cmake_case(cmake_flatten_package_path_containment_refusal
    FIXTURE flatten
    ORIGIN  tarball
    EXTRA   "${_cli}" ${_declare} -DFX_PACKAGE_PATH=../escape
    REFUSES "PACKAGE_PATH[ \t\r\n]+entry[ \t\r\n]+'[.][.]/escape'")

meios_cmake_case(cmake_flatten_args_option_refusal
    FIXTURE flatten
    ORIGIN  tarball
    EXTRA   "${_cli}" ${_declare} -DFX_ARGS=-prefix:=zz_
    REFUSES "begins[ \t\r\n]+with[ \t\r\n]+'-'")

# The entry is written with the fixture's own separator, so what the module sees is the two list
# elements a ';' inside a value produces, the second carrying no assignment at all.
meios_cmake_case(cmake_flatten_args_shape_refusal
    FIXTURE flatten
    ORIGIN  tarball
    EXTRA   "${_cli}" ${_declare} -DFX_ARGS=prefix:=zz_,tail
    REFUSES "is[ \t\r\n]+not[ \t\r\n]+key:=value")

# Both install-keyword patterns carry the calling function's own prefix, because the deploy call
# refuses the same two mistakes under the same words: a pattern without the prefix would be
# satisfied by the deploy module's message and stop pinning the guard the case names.
meios_cmake_case(cmake_flatten_install_component_alone_refusal
    FIXTURE flatten
    ORIGIN  tarball
    EXTRA   "${_cli}" ${_declare} -DFX_INSTALL_COMPONENT=descriptions
    REFUSES "meios_target_flatten_resource\\(app\\):[ \t\r\n]+INSTALL_COMPONENT[ \t\r\n]+requires[ \t\r\n]+INSTALL_DESTINATION")

meios_cmake_case(cmake_flatten_install_location_conflict_refusal
    FIXTURE flatten
    ORIGIN  tarball
    EXTRA   "${_cli}" ${_declare} -DFX_INSTALL_RUNTIME_RELATIVE=ON
            -DFX_INSTALL_DESTINATION=share/robot
    REFUSES "meios_target_flatten_resource\\(app\\):[ \t\r\n]+INSTALL_RUNTIME_RELATIVE[ \t\r\n]+and[ \t\r\n]+INSTALL_DESTINATION[ \t\r\n]+both")

endif()
