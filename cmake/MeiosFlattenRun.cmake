# Runs the meios CLI's flatten verb and publishes what it prints. Run in script mode:
#
#   cmake -DMEIOS_FLATTEN_CLI=<binary> -DMEIOS_FLATTEN_OUT=<file>
#         [-DMEIOS_FLATTEN_ARGS=<arg>[;<arg>...]] -P MeiosFlattenRun.cmake
#
# The flatten verb prints the flattened document on standard output and reports on standard error,
# so the build rule needs a captor rather than a redirection: a build command is verbatim by
# construction and a redirection character would reach the program as an argument.

cmake_minimum_required(VERSION 3.28)

if(NOT MEIOS_FLATTEN_CLI)
    message(FATAL_ERROR
        "MeiosFlattenRun: set MEIOS_FLATTEN_CLI to the meios binary to run.")
endif()
if(NOT MEIOS_FLATTEN_OUT)
    message(FATAL_ERROR
        "MeiosFlattenRun: set MEIOS_FLATTEN_OUT to the file to write.")
endif()

get_filename_component(_parent "${MEIOS_FLATTEN_OUT}" DIRECTORY)
if(_parent)
    file(MAKE_DIRECTORY "${_parent}")
endif()

# Publish atomically, as the acquisition paths do: build tools disagree about a failed command's
# leftovers — one deletes the output it names, another keeps whatever landed there — so capturing
# straight into the output would leave a truncated flattened document behind on the most common
# developer generator. Nothing is captured off standard error, which is what carries the CLI's
# typed diagnostic through to the build log.
set(_tmp "${MEIOS_FLATTEN_OUT}.tmp")
execute_process(
    COMMAND "${MEIOS_FLATTEN_CLI}" flatten ${MEIOS_FLATTEN_ARGS}
    OUTPUT_FILE     "${_tmp}"
    RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    file(REMOVE "${_tmp}")
    message(FATAL_ERROR
        "MeiosFlattenRun: flatten exited ${_rc}; nothing was written.")
endif()

file(RENAME "${_tmp}" "${MEIOS_FLATTEN_OUT}")
