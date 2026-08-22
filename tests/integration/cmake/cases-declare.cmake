# Every inter-word space is spelled as a character class because CMake re-wraps message(FATAL_ERROR)
# text at roughly 78 columns with a continuation indent, so a pattern copied verbatim out of the
# module's source string does not match what the case actually reads. The patterns stop short of any
# non-ASCII character in the message on purpose: the Windows console code page decides how one is
# rendered, and no assertion should depend on that.

meios_cmake_case(cmake_declare_name_refusal
    FIXTURE module
    EXTRA -DFX_NAME=bad/name
    REFUSES "NAME[ \t\r\n]+must[ \t\r\n]+be[ \t\r\n]+a[ \t\r\n]+plain[ \t\r\n]+identifier")

# No REFUSES and no MATCHES: the verdict is the driver's exit code, and the fixture's own
# post-conditions are what make this assert that the slice actually sliced rather than merely that
# a clone happened.
meios_cmake_case(cmake_declare_git_sparse
    FIXTURE module
    ORIGIN git
    EXTRA -DFX_NAME=demo -DFX_GIT_TAG=v1 -DFX_SPARSE_PATHS=pkg_a
          -DFX_REQUIRE_DIR=pkg_a -DFX_REQUIRE_ABSENT=pkg_b)

meios_cmake_case(cmake_declare_url_hash
    FIXTURE module
    ORIGIN tarball
    EXTRA -DFX_NAME=demo -DFX_REQUIRE_DIR=pkg_a,pkg_b)
