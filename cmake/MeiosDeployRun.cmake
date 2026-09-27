# Copies one deployed tree into place while holding the lock of the runtime directory it lands in.
# Run in script mode:
#
#   cmake -DMEIOS_DEPLOY_SOURCE=<dir> -DMEIOS_DEPLOY_DESTINATION=<dir>
#         -DMEIOS_STEP_ROOT=<runtime dir> -DMEIOS_STEP_BINARY_DIR=<top build dir>
#         [-DMEIOS_STEP_LOCK_TIMEOUT=<seconds>] -P MeiosDeployRun.cmake

cmake_minimum_required(VERSION 3.28)

include("${CMAKE_CURRENT_LIST_DIR}/MeiosStepLock.cmake")

foreach(_name IN ITEMS MEIOS_DEPLOY_SOURCE MEIOS_DEPLOY_DESTINATION MEIOS_STEP_ROOT
                       MEIOS_STEP_BINARY_DIR)
    if("${${_name}}" STREQUAL "")
        message(FATAL_ERROR "MeiosDeployRun: set ${_name}.")
    endif()
endforeach()

# Two targets built into one runtime directory deploy into one place, and two copies of one tree
# running at once fail each other.
_meios_step_lock("${MEIOS_STEP_BINARY_DIR}" "${MEIOS_STEP_ROOT}" "${MEIOS_STEP_LOCK_TIMEOUT}" _lock)
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
            "${MEIOS_DEPLOY_SOURCE}" "${MEIOS_DEPLOY_DESTINATION}"
    COMMAND_ERROR_IS_FATAL ANY)
file(LOCK "${_lock}" RELEASE)
