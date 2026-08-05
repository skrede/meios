# Nothing in the tree and no workflow ever added meios as a subdirectory, which is where
# FetchContent_MakeAvailable ends, so three of the option-gated trees resolved meios's own sources
# through whichever project happened to sit at the top of the build and nothing caught it.
#
# The build target every option-tree case names is the fixture's own executable. It links
# meios::urdf and its translation unit includes headers from both meios::urdf and meios::core, so
# building it proves the consumer's own public interface resolves -- compile and link -- under that
# configuration. It proves no more than that: the object set is identical under every option,
# because no MEIOS_BUILD_* option changes the consumer's dependency chain, so no option tree's own
# targets are compiled and the breadth these cases add over the default build-and-link case is
# configure-scoped. What binds the option-gated listfiles' own compile-level defects is still the
# include-root read-back inside the fixture.
#
# A case that names that target raises its time limit because it configures and compiles meios from
# scratch under a foreign source root, which the shared default is not sized for.

set(_meios_tree "-DFX_MEIOS_SOURCE_DIR=${meios_SOURCE_DIR}")

# The baseline: a consumer that sets no option at all still configures and gets meios::core. It
# names no build target, because the default configuration's build-and-link is already carried by
# the install opt-in case and a fifth compile of the same object set would buy nothing.
meios_cmake_case(cmake_subproject_default
    FIXTURE subproject
    DRIVER  meios_subproject_case.cmake
    EXTRA   ${_meios_tree})

# The CLI reached core's private source root through the top of the build. That configures green
# and fails to compile, so it is the fixture's include-root assertion that binds it here; the build
# target binds the consumer's own link against the public interface under this option.
meios_cmake_case(cmake_subproject_tools
    FIXTURE subproject
    DRIVER  meios_subproject_case.cmake
    EXTRA   ${_meios_tree} -DMEIOS_BUILD_TOOLS=ON
    BUILD_TARGET meios_subproject_consumer
    TIMEOUT 1800)

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
    DRIVER  meios_subproject_case.cmake
    EXTRA   ${_meios_tree} -DMEIOS_BUILD_TESTS=ON ${_meios_catch2}
    BUILD_TARGET meios_subproject_consumer
    TIMEOUT 1800)

# The snippet extractor and the README were looked for under the consumer's root. The interpreter
# this needs is the one the docs tree already requires.
meios_cmake_case(cmake_subproject_docs
    FIXTURE subproject
    DRIVER  meios_subproject_case.cmake
    EXTRA   ${_meios_tree} -DMEIOS_BUILD_DOCS=ON
    BUILD_TARGET meios_subproject_consumer
    TIMEOUT 1800)

# The examples tree names no top-of-build source path today, so this passes before and after the
# change. It exists so that no option tree is left unproven, and it goes red the moment one is added.
meios_cmake_case(cmake_subproject_examples
    FIXTURE subproject
    DRIVER  meios_subproject_case.cmake
    EXTRA   ${_meios_tree} -DMEIOS_BUILD_EXAMPLES=ON
    BUILD_TARGET meios_subproject_consumer
    TIMEOUT 1800)
