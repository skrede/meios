# Two claims about the module wiring were proven only by hand probes that were reverted: that a
# module can be withheld from the installed export only with a recorded reason, and that a withheld
# module stays out of the export wherever it is declared relative to the export install call.
#
# Every pattern here spells its inter-word spaces as a whitespace character class for the reason
# given at the head of cases-refusal.cmake.

set(_module_wiring "-DFX_MODULES_FILE=${meios_SOURCE_DIR}/lib/modules.cmake")
set(_module_refusal "marked[ \t\r\n]+NOT_EXPORTABLE[ \t\r\n]+with[ \t\r\n]+no[ \t\r\n]+reason")

# A reason spelled as the empty string and a marker carrying no value at all are two distinct
# mistakes reaching the same guard, and only one of them is what cmake_parse_arguments reports.
meios_cmake_case(cmake_module_withheld_reason_empty
    FIXTURE module-export
    EXTRA   ${_module_wiring} -DFX_WITHOUT_REASON=empty
    REFUSES "${_module_refusal}")

meios_cmake_case(cmake_module_withheld_reason_absent
    FIXTURE module-export
    EXTRA   ${_module_wiring} -DFX_WITHOUT_REASON=absent
    REFUSES "${_module_refusal}")

# The export set is assembled at generate time, so where a module is declared relative to the
# install(EXPORT) call decides nothing -- which is a claim about CMake that only an installed export
# can settle. The kept module declared after that call is the control: it must be present, or the
# withheld pair's absence would be explained by position rather than by the marker.
meios_cmake_case(cmake_module_withheld_regardless_of_order
    FIXTURE module-export
    DRIVER  meios_subproject_case.cmake
    EXTRA   ${_module_wiring} -DFX_EXPORT_ORDER=ON
    INSTALL ON
    REQUIRE_CONTAINS "${CMAKE_INSTALL_LIBDIR}/cmake/meios/meiosTargets.cmake,meios::kept-late"
    REQUIRE_PREFIX_LACKS "${CMAKE_INSTALL_LIBDIR}/cmake/meios/meiosTargets.cmake,meios::withheld-")
