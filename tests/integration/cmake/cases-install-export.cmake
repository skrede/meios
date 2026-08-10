# One optional backend used to suppress everybody else's install, and the two lists that decide
# what an installed package carries were maintained by hand. These cases hold the replacements to
# their contract from the only side that can judge them: a consumer configured against a prefix.
#
# Every pattern here spells its inter-word spaces as a whitespace character class for the reason
# given at the head of cases-refusal.cmake.
#
# Each case configures, compiles and installs meios from scratch and then configures a consumer
# against the staged prefix, which is four tree operations behind one time limit, so each raises
# that limit explicitly.

set(_export_tree "-DFX_MEIOS_SOURCE_DIR=${meios_SOURCE_DIR}")

# pugixml is told to install itself so the staged prefix is self-contained whether or not the host
# running these cases has a discoverable one, and both enrichments that default on are off so the
# only compiled install rules are the ones the named build target really produces.
set(_export_build ${_export_tree} -DFX_PARENT_INSTALL=ON -DPUGIXML_INSTALL=ON
                  -DMEIOS_ROS_PACKAGE_SUPPORT=OFF -DMEIOS_YAML_SUPPORT=OFF)

set(_export_cmakedir "${CMAKE_INSTALL_LIBDIR}/cmake/meios")
set(_export_installed "${_export_cmakedir}/meiosConfig.cmake")
string(APPEND _export_installed ",${_export_cmakedir}/MeiosResourcePaths.cmake")
string(APPEND _export_installed
    ",${CMAKE_INSTALL_LIBDIR}/${CMAKE_STATIC_LIBRARY_PREFIX}meios_core${CMAKE_STATIC_LIBRARY_SUFFIX}")

# The withheld module's own archive and its own header directory, which no other module installs.
set(_export_withheld
    "${CMAKE_INSTALL_LIBDIR}/${CMAKE_STATIC_LIBRARY_PREFIX}meios_eval-python${CMAKE_STATIC_LIBRARY_SUFFIX}")
string(APPEND _export_withheld ",${CMAKE_INSTALL_INCLUDEDIR}/meios/eval")

# A required component the installed export does not carry is refused by the package config setting
# its own found flag false, which is the mechanism the config already runs for every component and
# the one message that reaches the consumer.
set(_export_refusal "meios_FOUND[ \t\r\n]+to[ \t\r\n]+FALSE")
set(_export_component_request -DFX_FIND_PACKAGE=ON -DFX_COMPONENTS=eval-python -DFX_NAME=demo)
list(APPEND _export_component_request
    -DFX_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/origin/clean)

# The same request with the component clause removed. It is what makes the refusal above
# discriminating: an unresolvable find_dependency(pugixml) prints that same sentence and would fail
# both requests, so the case only stands when the componentless one succeeds against the same prefix.
set(_export_control -DFX_FIND_PACKAGE=ON -DFX_NAME=demo)
list(APPEND _export_control -DFX_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/origin/clean)

# Built and withheld. The command-line tool links the backend privately onto a support library
# that is in no export set, so the tool still installs and carries the capability the config
# records; the backend's own archive and headers are absent together.
if(TARGET meios_eval-python)
    meios_cmake_case(cmake_install_withholds_backend
        FIXTURE subproject
        DRIVER  meios_subproject_case.cmake
        EXTRA   ${_export_build} -DMEIOS_BUILD_TOOLS=ON -DMEIOS_EVAL_PYTHON_SUPPORT=ON
                -DMEIOS_REQUIRE_EVAL_PYTHON=ON
        BUILD_TARGET meios
        INSTALL ON
        TIMEOUT 1800
        REQUIRE_INSTALLED ${_export_installed},${CMAKE_INSTALL_BINDIR}/meios${CMAKE_EXECUTABLE_SUFFIX}
        REQUIRE_PREFIX_ABSENT ${_export_withheld}
        REQUIRE_CONTAINS "${_export_cmakedir}/meiosConfig.cmake,MEIOS_CLI_HAS_EVAL_PYTHON[ \t\r\n]+TRUE"
        CONSUMER module
        CONSUMER_EXTRA ${_export_component_request}
        CONSUMER_CONTROL ${_export_control}
        REFUSES "${_export_refusal}")
endif()

# Never built. The two absences have to be indistinguishable to a consumer and both loud, so this
# one binds the same refusal against a prefix the backend never entered.
meios_cmake_case(cmake_install_backend_absent
    FIXTURE subproject
    DRIVER  meios_subproject_case.cmake
    EXTRA   ${_export_build}
    BUILD_TARGET meios_core
    INSTALL ON
    TIMEOUT 1800
    CONSUMER module
    CONSUMER_EXTRA ${_export_component_request}
    CONSUMER_CONTROL ${_export_control}
    REFUSES "${_export_refusal}")

# A build-tree alias is never exported, so the spelling a consumer finally writes exists only in the
# installed targets file and in the config's component roster. Neither is observable from the
# working tree, which is why this one stages a prefix and reads both back.
#
# The shared build set turns this enrichment off; this case is the one that needs it built, so it
# states the option itself rather than appending a contradictory second -D to that set.
set(_export_package_build ${_export_tree} -DFX_PARENT_INSTALL=ON -DPUGIXML_INSTALL=ON
                          -DMEIOS_ROS_PACKAGE_SUPPORT=ON -DMEIOS_YAML_SUPPORT=OFF)

# The module's own directory and the include root it ships deliberately disagree, so the installed
# header path is exactly what a directory move could carry off without anything noticing.
set(_export_package_installed "${_export_installed}")
string(APPEND _export_package_installed
    ",${CMAKE_INSTALL_LIBDIR}/${CMAKE_STATIC_LIBRARY_PREFIX}meios_ros-package${CMAKE_STATIC_LIBRARY_SUFFIX}"
    ",${CMAKE_INSTALL_INCLUDEDIR}/meios/ros/ros_package_source.h")

set(_export_package_request -DFX_FIND_PACKAGE=ON -DFX_COMPONENTS=ros-package -DFX_NAME=demo)
list(APPEND _export_package_request
    -DFX_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/origin/clean)

meios_cmake_case(cmake_install_exports_package_resolution_component
    FIXTURE subproject
    DRIVER  meios_subproject_case.cmake
    EXTRA   ${_export_package_build}
    BUILD_TARGET meios_ros-package
    INSTALL ON
    TIMEOUT 1800
    REQUIRE_INSTALLED ${_export_package_installed}
    REQUIRE_CONTAINS "${_export_cmakedir}/meiosTargets.cmake,meios::ros-package"
    CONSUMER module
    CONSUMER_EXTRA ${_export_package_request}
    CONSUMER_CONTROL ${_export_control})

# The auxiliary-format module is the one enrichment a default build produces, and it is also the
# one whose private link edge on a static archive survives into the exported interface. So this
# stages a prefix with it built and reads back three things a working tree cannot show: the archive
# and its header arrived, the export carries the namespaced spelling, and a consumer configured
# against the prefix resolves the dependency the config declares for it.
set(_export_auxiliary_build ${_export_tree} -DFX_PARENT_INSTALL=ON -DPUGIXML_INSTALL=ON
                            -DMEIOS_ROS_PACKAGE_SUPPORT=OFF -DMEIOS_YAML_SUPPORT=ON)

set(_export_auxiliary_installed "${_export_installed}")
string(APPEND _export_auxiliary_installed
    ",${CMAKE_INSTALL_LIBDIR}/${CMAKE_STATIC_LIBRARY_PREFIX}meios_yaml${CMAKE_STATIC_LIBRARY_SUFFIX}"
    ",${CMAKE_INSTALL_INCLUDEDIR}/meios/yaml/parser.h"
    ",${CMAKE_INSTALL_INCLUDEDIR}/meios/config.h")

set(_export_auxiliary_request -DFX_FIND_PACKAGE=ON -DFX_COMPONENTS=yaml -DFX_NAME=demo)
list(APPEND _export_auxiliary_request
    -DFX_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/origin/clean)

meios_cmake_case(cmake_install_exports_auxiliary_format_component
    FIXTURE subproject
    DRIVER  meios_subproject_case.cmake
    EXTRA   ${_export_auxiliary_build}
    BUILD_TARGET meios_yaml
    INSTALL ON
    TIMEOUT 1800
    REQUIRE_INSTALLED ${_export_auxiliary_installed}
    REQUIRE_CONTAINS "${_export_cmakedir}/meiosTargets.cmake,meios::yaml"
    CONSUMER module
    CONSUMER_EXTRA ${_export_auxiliary_request}
    CONSUMER_CONTROL ${_export_control})

# The path-containment check is a traversal control, and its absence from a prefix is invisible to
# every in-tree test. What is asserted is not that a list agrees -- the derivation makes disagreement
# structurally impossible -- but that a consumer configured against the prefix reaches the control
# and is refused by it.
meios_cmake_case(cmake_install_containment_reachable
    FIXTURE subproject
    DRIVER  meios_subproject_case.cmake
    EXTRA   ${_export_build}
    BUILD_TARGET meios_core
    INSTALL ON
    TIMEOUT 1800
    CONSUMER module
    CONSUMER_EXTRA -DFX_FIND_PACKAGE=ON -DFX_NAME=demo
                   -DFX_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/origin/clean -DFX_SUBDIR=../..
    REFUSES "escapes[ \t\r\n]+the[ \t\r\n]+acquired[ \t\r\n]+tree")

# A module reached by nothing and named by nobody is how a hand-maintained list loses an entry and
# how a glob ships one nobody wired. The orphan is introduced into a copy of the module directory,
# so the tree the rest of the suite is reading stays untouched.
meios_cmake_case(cmake_module_list_orphan
    FIXTURE modules
    EXTRA   -DFX_MODULES_FROM=${meios_SOURCE_DIR}/cmake -DFX_ORPHAN=MeiosOrphan.cmake
    REFUSES "MeiosOrphan\\.cmake[ \t\r\n]+is[ \t\r\n]+reached[ \t\r\n]+by[ \t\r\n]+no")
