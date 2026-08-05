# Nothing in the tree and no workflow ever added meios as a subdirectory, which is where
# FetchContent_MakeAvailable ends, so three of the option-gated trees resolved meios's own sources
# through whichever project happened to sit at the top of the build and nothing caught it.

set(_meios_tree "-DFX_MEIOS_SOURCE_DIR=${meios_SOURCE_DIR}")

# The baseline: a consumer that sets no option at all still configures and gets meios::core.
meios_cmake_case(cmake_subproject_default
    FIXTURE subproject
    EXTRA   ${_meios_tree})

# The CLI reached core's private source root through the top of the build. That configures green
# and fails to compile, so it is the fixture's include-root assertion that binds it here.
meios_cmake_case(cmake_subproject_tools
    FIXTURE subproject
    EXTRA   ${_meios_tree} -DMEIOS_BUILD_TOOLS=ON)

# The test tree included the corpus module through the top of the build, so this case refused at
# configure with the module not found. The sub-configure runs meios's own test tree and so needs a
# Catch2; handing it the one this build discovered saves it cloning its own, and where this build
# fetched instead there is nothing to hand over and the sub-configure fetches.
set(_meios_catch2 "")
if(Catch2_DIR)
    set(_meios_catch2 "-DCatch2_DIR=${Catch2_DIR}")
endif()
meios_cmake_case(cmake_subproject_tests
    FIXTURE subproject
    EXTRA   ${_meios_tree} -DMEIOS_BUILD_TESTS=ON ${_meios_catch2})

# The snippet extractor and the README were looked for under the consumer's root. The interpreter
# this needs is the one the docs tree already requires.
meios_cmake_case(cmake_subproject_docs
    FIXTURE subproject
    EXTRA   ${_meios_tree} -DMEIOS_BUILD_DOCS=ON)

# The examples tree names no top-of-build source path today, so this passes before and after the
# change. It exists so that no option tree is left unproven, and it goes red the moment one is added.
meios_cmake_case(cmake_subproject_examples
    FIXTURE subproject
    EXTRA   ${_meios_tree} -DMEIOS_BUILD_EXAMPLES=ON)
