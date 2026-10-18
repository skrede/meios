cmake_minimum_required(VERSION 3.28)

include("${CMAKE_CURRENT_LIST_DIR}/oracle_digest_gate.cmake")

# Each case breaks a scratch copy of the committed records in one way and runs the same gate the
# configure runs, so a refusal that stopped firing would turn its case red.
file(REMOVE_RECURSE "${WORK}")
file(COPY "${ORACLE_DIR}/" DESTINATION "${WORK}")
file(GLOB records RELATIVE "${WORK}" "${WORK}/*.cases")
list(GET records 0 victim)
if(CASE STREQUAL "drifted")
    file(APPEND "${WORK}/${victim}" "# drifted\n")
elseif(CASE STREQUAL "missing")
    file(REMOVE "${WORK}/${victim}")
elseif(CASE STREQUAL "unlisted")
    file(WRITE "${WORK}/unlisted_facts.cases" "x\t1\n")
elseif(NOT CASE STREQUAL "clean")
    message(FATAL_ERROR "oracle gate case: unknown case '${CASE}'")
endif()
meios_check_oracle_records("${WORK}" rows)
if(NOT rows GREATER 0)
    message(FATAL_ERROR "oracle gate case: the clean copy counted no rows")
endif()
message(STATUS "MEIOS_ORACLE_GATE_PASSED rows=${rows}")
