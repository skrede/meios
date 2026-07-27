# Every inter-word space is a character class for the reason given in cases-declare.cmake: CMake
# re-wraps message(FATAL_ERROR) text at roughly 78 columns, so a pattern copied verbatim out of the
# module's source string does not match what the case reads. Each pattern also stops short of any
# non-ASCII character in the message, whose rendering the console code page decides.

meios_cmake_case(cmake_declare_sparse_typo_refusal
    FIXTURE module
    ORIGIN git
    EXTRA -DFX_NAME=demo -DFX_GIT_TAG=v1 -DFX_SPARSE_PATHS=pkg_TYPO
    REFUSES "SPARSE_PATHS[ \t\r\n]+selected[ \t\r\n]+nothing[ \t\r\n]+for")

meios_cmake_case(cmake_declare_lfs_pointer_refusal
    FIXTURE module
    ORIGIN git-lfs
    EXTRA -DFX_NAME=demo -DFX_GIT_TAG=v1
    REFUSES "unsmudged[ \t\r\n]+Git-LFS[ \t\r\n]+pointers[ \t\r\n]+found")

# The escaped tree has to be a real directory, because the check resolves both sides before
# comparing them; the committed origin is the nearest one, named at registration time.
meios_cmake_case(cmake_declare_subdir_escape_refusal
    FIXTURE module
    EXTRA -DFX_NAME=demo -DFX_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/origin/clean
          -DFX_SUBDIR=../..
    REFUSES "escapes[ \t\r\n]+the[ \t\r\n]+acquired[ \t\r\n]+tree")

# This one never reaches git, and that is the property: the containment check on the sparse path
# list runs before the acquisition mode is even counted, so the unreachable repository is never
# contacted.
meios_cmake_case(cmake_declare_sparse_containment_refusal
    FIXTURE module
    EXTRA -DFX_NAME=demo -DFX_GIT_REPOSITORY=file:///nonexistent -DFX_SPARSE_PATHS=../escape
    REFUSES "relative[ \t\r\n]+path[ \t\r\n]+inside[ \t\r\n]+the[ \t\r\n]+tree")
