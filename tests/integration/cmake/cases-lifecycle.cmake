# No REFUSES and no MATCHES: every assertion lives inside the driver, which walks one ordered
# sequence over two build trees, so the driver's exit code is the verdict.
meios_cmake_case(cmake_prune_two_trees
    FIXTURE lifecycle
    DRIVER meios_prune_case.cmake)
