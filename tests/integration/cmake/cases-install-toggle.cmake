# Nothing held meios to the claim that a consumer's install carries only what the consumer asked
# for. The install was gated on a variable no consumer could set, and two unrelated conditions
# forced it off without saying so, which surfaced days later as a generate-time export error
# inside somebody else's project.
#
# Every pattern here spells its inter-word spaces as a whitespace character class for the reason
# given at the head of cases-refusal.cmake; status text is re-wrapped exactly as fatal-error text
# is. Each binds through MATCHES rather than REFUSES because the sub-configure succeeds in all
# four cases and it is a status line, not a refusal, that carries the reason.

set(_toggle_tree "-DFX_MEIOS_SOURCE_DIR=${meios_SOURCE_DIR}")

# The archive is the one asserted path whose spelling the platform decides, so it is composed from
# the toolchain's own prefix and suffix rather than written out.
set(_toggle_archive
    "${CMAKE_INSTALL_LIBDIR}/${CMAKE_STATIC_LIBRARY_PREFIX}meios_core${CMAKE_STATIC_LIBRARY_SUFFIX}")
set(_toggle_installed "${CMAKE_INSTALL_LIBDIR}/cmake/meios/meiosConfig.cmake")
string(APPEND _toggle_installed ",${CMAKE_INSTALL_LIBDIR}/cmake/meios/meiosConfigVersion.cmake")
string(APPEND _toggle_installed ",${CMAKE_INSTALL_LIBDIR}/cmake/meios/MeiosDeclareResource.cmake")
string(APPEND _toggle_installed ",${_toggle_archive}")

# The requirement itself: a consumer that asked for nothing receives nothing. This one needs no
# build, because with the toggle off no install rule is generated at all, so the install step
# succeeds trivially and the prefix stays empty.
meios_cmake_case(cmake_subproject_install_declined
    FIXTURE subproject
    DRIVER  meios_subproject_case.cmake
    EXTRA   ${_toggle_tree}
    INSTALL ON
    REQUIRE_PREFIX_EMPTY ON
    MATCHES "meios[ \t\r\n]+was[ \t\r\n]+added[ \t\r\n]+as[ \t\r\n]+a[ \t\r\n]+subproject")

# The consumer's listfile has to beat a command-line cache entry of the same name, and it has to
# still beat it when the same tree is configured a second time. The parent's value is spelled in
# lowercase, so the case also pins that the toggle is read through CMake's own truthiness rather
# than compared as a string. pugixml's own install option is turned on because a fetched pugixml
# that installs nothing belongs to no export set: without it this case would pass on a host that
# has a discoverable pugixml and fail on one that does not -- and it is a consumer setting that
# option, which is exactly the choice meios stopped taking on the consumer's behalf.
# A case that compiles and installs meios from scratch needs more than the shared time limit, so
# this one raises it explicitly.
meios_cmake_case(cmake_subproject_install_optin
    FIXTURE subproject
    DRIVER  meios_subproject_case.cmake
    EXTRA   ${_toggle_tree} -DFX_PARENT_INSTALL=on -DMEIOS_INSTALL=OFF
            -DPUGIXML_INSTALL=ON -DMEIOS_ROS_PACKAGE_SUPPORT=OFF
    SECOND_CONFIGURE ${_toggle_tree}
    BUILD_TARGET meios_subproject_consumer
    INSTALL ON
    TIMEOUT 1800
    REQUIRE_INSTALLED ${_toggle_installed}
    MATCHES "installing[ \t\r\n]+meios[ \t\r\n]+and[ \t\r\n]+its[ \t\r\n]+CMake")

# The opt-in above passes -DMEIOS_INSTALL=OFF on purpose and so writes a cache entry; this one
# passes nothing, which is the only shape in which the claim is checkable. CMP0077 is what makes it
# true: option() honors the consumer's normal variable and writes no cache entry over it, so the
# decision stays the consumer listfile's to change and no first configure freezes it.
# PUGIXML_INSTALL is turned on for the reason the opt-in above states, and only that option is
# passed: it writes no MEIOS_INSTALL cache entry, so it leaves the claim under test alone.
meios_cmake_case(cmake_subproject_install_optin_uncached
    FIXTURE subproject
    DRIVER  meios_subproject_case.cmake
    EXTRA   ${_toggle_tree} -DFX_PARENT_INSTALL=on -DMEIOS_ROS_PACKAGE_SUPPORT=OFF
            -DPUGIXML_INSTALL=ON
    REQUIRE_TREE_LACKS "CMakeCache.txt,(^|\n)MEIOS_INSTALL:"
    MATCHES "installing[ \t\r\n]+meios[ \t\r\n]+and[ \t\r\n]+its[ \t\r\n]+CMake")

# Ordering is the whole content: a consumer that sets the toggle after the add_subdirectory has
# already missed the decision, and nothing but the printed reason tells it so.
meios_cmake_case(cmake_subproject_install_optin_too_late
    FIXTURE subproject
    DRIVER  meios_subproject_case.cmake
    EXTRA   ${_toggle_tree} -DFX_PARENT_INSTALL_LATE=on
    INSTALL ON
    REQUIRE_PREFIX_EMPTY ON
    MATCHES "setting[ \t\r\n]+it[ \t\r\n]+afterwards[ \t\r\n]+is[ \t\r\n]+too[ \t\r\n]+late")

# The suppression this replaces tested one dependency's source directory, so an enrichment whose
# dependency is not the XML parser was a generate-time export failure with no message and no test.
# The fetch path is forced so which dependencies are acquired does not depend on the host, which
# also puts an acquisition on the critical path where the other declined-install cases have none,
# and that is what the raised time limit covers.
meios_cmake_case(cmake_install_reason_fetched_dependency
    FIXTURE subproject
    DRIVER  meios_subproject_case.cmake
    EXTRA   ${_toggle_tree} -DFX_PARENT_INSTALL=ON -DMEIOS_BUILD_ARCHIVE_ZIP=ON
            -DMEIOS_CMAKE_FETCH_DEPS=ON
    INSTALL ON
    TIMEOUT 900
    REQUIRE_PREFIX_EMPTY ON
    MATCHES "miniz[ \t\r\n]+\\(turn[ \t\r\n]+INSTALL_PROJECT")

# The CMake resource modules an install carries were checked once by hand against a recursive
# listing of the prefix. A frozen listing would churn on every legitimate install change; this
# binds the prefix to the derivation the build itself installs from, so a module that stops
# reaching the prefix reddens the case while one that is added updates the derivation and does not.
# The opposite direction -- a file in the prefix that nothing wired -- is already refused at
# configure by cmake_module_list_orphan, which is why only this direction is asserted here.
meios_installed_cmake_modules("${meios_SOURCE_DIR}/cmake" _toggle_derived)
set(_toggle_manifest "${CMAKE_INSTALL_LIBDIR}/cmake/meios/meiosConfig.cmake")
foreach(_module IN LISTS _toggle_derived)
    get_filename_component(_name "${_module}" NAME)
    string(APPEND _toggle_manifest ",${CMAKE_INSTALL_LIBDIR}/cmake/meios/${_name}")
endforeach()

meios_cmake_case(cmake_subproject_install_manifest
    FIXTURE subproject
    DRIVER  meios_subproject_case.cmake
    EXTRA   ${_toggle_tree} -DFX_PARENT_INSTALL=ON -DPUGIXML_INSTALL=ON
            -DMEIOS_ROS_PACKAGE_SUPPORT=OFF
    BUILD_TARGET meios_core
    INSTALL ON
    TIMEOUT 1800
    REQUIRE_INSTALLED ${_toggle_manifest})
