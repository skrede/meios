# The driver's tarball origin already supplied the correct -DFX_HASH=; repeating it here places the
# wrong value later on the sub-configure's command line, and the later assignment is what wins.
#
# The assertion binds to CMake's own `file DOWNLOAD HASH mismatch`, not to anything the module says:
# file(DOWNLOAD ... EXPECTED_HASH) treats a mismatch as a fatal error and aborts even when STATUS is
# supplied, so the module's status-code branch never runs and has no message to bind to.
meios_cmake_case(cmake_declare_hash_mismatch_refusal
    FIXTURE module
    ORIGIN tarball
    EXTRA -DFX_NAME=demo
          -DFX_HASH=SHA256=0000000000000000000000000000000000000000000000000000000000000000
    REFUSES "file[ \t\r\n]+DOWNLOAD[ \t\r\n]+HASH[ \t\r\n]+mismatch")

# MATCHES rather than REFUSES: this configure must succeed while printing the warning, and MATCHES
# pairs the pattern with a fail regex on the harness failure sentinel, so a run that warned and then
# fell over still scores as a failure. An empty FX_HASH is what makes the fixture omit the keyword.
meios_cmake_case(cmake_declare_unpinned_url_warning
    FIXTURE module
    ORIGIN tarball
    EXTRA -DFX_NAME=demo -DFX_HASH= -DFX_REQUIRE_DIR=pkg_a
    MATCHES "fetched[ \t\r\n]+WITHOUT[ \t\r\n]+an[ \t\r\n]+integrity[ \t\r\n]+hash")

# The marker, not a timestamp, is the assertion: acquiring an entry removes the whole directory
# before publishing the fresh one, so a file the first configure wrote into the tree surviving the
# second is exact proof that no re-acquire happened, and unlike a modification time it cannot be
# blurred by filesystem granularity or by a copy that preserves times.
meios_cmake_case(cmake_declare_stamp_reuse
    FIXTURE module
    ORIGIN tarball
    EXTRA -DFX_NAME=demo -DFX_MARK=harness_marker.txt
    SECOND_CONFIGURE -DFX_MARK= -DFX_REQUIRE_FILE=harness_marker.txt)
