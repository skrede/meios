cmake_minimum_required(VERSION 3.28)

include("${CMAKE_CURRENT_LIST_DIR}/meios_cmake_harness.cmake")

meios_harness_reset()

# The consumer project is configured exactly as a continuous-integration leg configures it, so the
# refusal under test is the one a misconfigured leg would meet, reached past the package lookup.
meios_harness_configure("${HARNESS_DIR}/../consumer" "${WORK}/consumer" "${EXTRA}" _rc)
meios_harness_sentinel("${_rc}")
