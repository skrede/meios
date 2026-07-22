include_guard(GLOBAL)

# Pinned, license-recorded robot-description corpus, fetched as DATA through
# meios_declare_resource (parsed, never compiled, never linked). The always-on
# tier fetches one small pinned known-good description plus an in-repo crafted
# known-faulty fixture wired via SOURCE_DIR; the breadth tier adds the full set.

option(MEIOS_FETCH_CORPUS
    "Fetch the pinned robot-description corpus and build the corpus test" OFF)
option(MEIOS_CORPUS_BREADTH
    "Fetch the full-breadth corpus for the non-blocking nightly tier" OFF)

# License determinations for every fetched upstream. Each corpus fetch NAME must
# carry a matching "# license[<name>]:" line below; the check further down reads
# this very file, extracts every meios_declare_resource NAME, and FATALs if any
# lacks its annotation — so a future fetch added without a recorded license fails
# configure rather than shipping an undetermined license into CI.
#
# # license[kuka_experimental]: Apache-2.0 (ros-industrial/kuka_experimental, root LICENSE)
# # license[ur_description]: BSD-3-Clause (UniversalRobots/Universal_Robots_ROS2_Description, root LICENSE)
# # license[lbr_fri_ros2_stack]: Apache-2.0 (lbr-stack/lbr_fri_ros2_stack, root LICENSE)
# # license[corpus_faulty]: crafted in-repo fixture (project-owned)

set(_corpus_self "")
file(READ "${CMAKE_CURRENT_LIST_FILE}" _corpus_self)
string(REGEX MATCHALL "meios_declare_resource\\([ \t\r\n]+NAME[ \t]+[A-Za-z0-9_]+"
       _corpus_calls "${_corpus_self}")
foreach(_call IN LISTS _corpus_calls)
    string(REGEX REPLACE ".*NAME[ \t]+([A-Za-z0-9_]+)$" "\\1" _cname "${_call}")
    if(NOT _corpus_self MATCHES "# license\\[${_cname}\\]:")
        message(FATAL_ERROR
            "corpus: fetch entry '${_cname}' has no '# license[${_cname}]:' determination. "
            "Every fetched upstream must record a machine-checkable license before it enters CI.")
    endif()
endforeach()

if(NOT MEIOS_FETCH_CORPUS)
    return()
endif()

# The known-good half of the pair: one small, pre-expanded serial-arm URDF
# that parses with zero error-level diagnostics (its package:// meshes surface as
# warn-level unresolved_mesh, never errors).
meios_declare_resource(
    NAME kuka_experimental
    URL  https://github.com/ros-industrial/kuka_experimental/archive/8d9292b04a22628b1b78d989e2ddd3abb913bf92.tar.gz
    HASH SHA256=02f299d967868fb32022617429ddede3a197fb8f18e09b88f88732e11a78bf79
    STRIP_TOP_LEVEL
    OUT_DIR MEIOS_CORPUS_KUKA_DIR)  # kuka_experimental melodic-devel (8d9292b04a22628b1b78d989e2ddd3abb913bf92)

set(MEIOS_CORPUS_KNOWN_GOOD
    "${MEIOS_CORPUS_KUKA_DIR}/kuka_lbr_iiwa_support/urdf/lbr_iiwa_14_r820.urdf")

# The known-faulty half: a crafted in-repo fixture wired through the fetcher's
# SOURCE_DIR override (no network, still runs the LFS scan). faulty.urdf declares
# two roots so topology emits exactly one additional_root at the second root's
# line — the fixed (code, file, line) the always-on tier pins.
meios_declare_resource(
    NAME corpus_faulty
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/tests/fixtures/urdf/corpus_faulty"
    OUT_DIR MEIOS_CORPUS_FAULTY_DIR)

set(MEIOS_CORPUS_FAULTY "${MEIOS_CORPUS_FAULTY_DIR}/faulty.urdf")
# faulty.urdf: <robot> on line 2, root_b (the additional root) declared on line 4.
set(MEIOS_CORPUS_FAULTY_LINE 4)

# The breadth tier adds the two larger upstreams; it runs only in the non-blocking
# nightly leg so a transient upstream outage never gates a PR.
set(MEIOS_CORPUS_BREADTH_DIRS "${MEIOS_CORPUS_KUKA_DIR}")
if(MEIOS_CORPUS_BREADTH)
    meios_declare_resource(
        NAME ur_description
        URL  https://github.com/UniversalRobots/Universal_Robots_ROS2_Description/archive/refs/tags/4.3.1.tar.gz
        HASH SHA256=3532a25c9942bedcdfe41c845be6d144bbbbcd76f179f14a53a3e7b6972ae72d
        STRIP_TOP_LEVEL
        OUT_DIR MEIOS_CORPUS_UR_DIR)  # ur_description 4.3.1

    meios_declare_resource(
        NAME lbr_fri_ros2_stack
        URL  https://github.com/lbr-stack/lbr_fri_ros2_stack/archive/refs/tags/jazzy-v2.5.0.tar.gz
        HASH SHA256=6d99bac113044642b8471690e2ceb079fe7c4624f19d7a01a000abb7b2430300
        STRIP_TOP_LEVEL
        OUT_DIR MEIOS_CORPUS_LBR_DIR)  # lbr_fri_ros2_stack jazzy-v2.5.0

    list(APPEND MEIOS_CORPUS_BREADTH_DIRS "${MEIOS_CORPUS_UR_DIR}" "${MEIOS_CORPUS_LBR_DIR}")
endif()
