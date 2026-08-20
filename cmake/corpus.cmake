include_guard(GLOBAL)

# Pinned, license-recorded robot-description corpus, fetched as DATA through
# meios_declare_resource (parsed, never compiled, never linked). The always-on
# tier fetches one small pinned known-good description plus an in-repo crafted
# known-faulty fixture wired via SOURCE_DIR; the breadth tier adds the remaining
# top-level documents that same upstream ships.

include("${CMAKE_CURRENT_LIST_DIR}/corpus_license.cmake")

option(MEIOS_FETCH_CORPUS
    "Fetch the pinned robot-description corpus and build the corpus test" OFF)
option(MEIOS_CORPUS_BREADTH
    "Load every top-level document the pinned known-good upstream ships" OFF)

# License determinations for every fetched upstream. Each corpus fetch NAME must
# carry a matching "# license[<name>]:" line below; the check further down reads
# this very file, extracts every meios_declare_resource NAME, and FATALs if any
# lacks its annotation — so a future fetch added without a recorded license fails
# configure rather than shipping an undetermined license into CI. The line's leading
# token is the canonical identifier and the parenthetical after it is provenance; an
# upstream whose manifest declares some other spelling records that spelling in the
# parenthetical too, and states it at its check call.
#
# # license[kuka_experimental]: Apache-2.0 (ros-industrial/kuka_experimental, root LICENSE)
# # license[ur_description]: BSD-3-Clause (UniversalRobots/Universal_Robots_ROS2_Description, root LICENSE)
# # license[lbr_med14_r820_description]: Apache-2.0 (lbr-stack/med14_r820_description, package.xml <license>, no root LICENSE)
# # license[franka_description]: Apache-2.0 (frankarobotics/franka_description, package.xml <license> declares "Apache 2.0")
# # license[corpus_faulty]: crafted in-repo fixture (project-owned)

# lbr_fri_ros2_stack is deliberately not fetched and not listed, so it carries no
# determination above. Its published tarball ships no robot description of any kind:
# every top-level document in it includes $(find lbr_iiwa14_r820_description) and
# siblings, and no such package is inside the tarball, so nothing there loads.
# Counting it as coverage it does not provide is worse than not fetching it. The
# description packages themselves are a separate upstream and would need their own
# pin and their own license determination.

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

# The known-good half of the pair: one small, pre-expanded serial-arm URDF that parses
# with zero error-level diagnostics. Its package:// meshes are not deployed, so that
# holds only because the harness builds its own context asking for warn-level
# unresolved_asset; the library itself refuses an unresolved asset by default.
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
    SOURCE_DIR "${meios_SOURCE_DIR}/tests/fixtures/urdf/corpus_faulty"
    OUT_DIR MEIOS_CORPUS_FAULTY_DIR)

set(MEIOS_CORPUS_FAULTY "${MEIOS_CORPUS_FAULTY_DIR}/faulty.urdf")
# faulty.urdf: <robot> on line 2, root_b (the additional root) declared on line 4.
set(MEIOS_CORPUS_FAULTY_LINE 4)

meios_declare_resource(
    NAME ur_description
    URL  https://github.com/UniversalRobots/Universal_Robots_ROS2_Description/archive/refs/tags/4.3.1.tar.gz
    HASH SHA256=3532a25c9942bedcdfe41c845be6d144bbbbcd76f179f14a53a3e7b6972ae72d
    STRIP_TOP_LEVEL
    OUT_DIR MEIOS_CORPUS_UR_DIR)  # ur_description 4.3.1
meios_corpus_check_license(ur_description "${MEIOS_CORPUS_UR_DIR}/package.xml" "${_corpus_self}"
    "BSD-3-Clause")

meios_declare_resource(
    NAME lbr_med14_r820_description
    URL  https://github.com/lbr-stack/med14_r820_description/archive/refs/tags/v2.5.0.tar.gz
    HASH SHA256=edb596d3e2b7f07f5b8f66ef68cd3ccfef0212e98efe25742867cfbe653deb5a
    STRIP_TOP_LEVEL
    OUT_DIR MEIOS_CORPUS_LBR_DIR)  # lbr_med14_r820_description 2.5.0
meios_corpus_check_license(lbr_med14_r820_description "${MEIOS_CORPUS_LBR_DIR}/package.xml"
    "${_corpus_self}" "Apache-2.0")

# frankaemika/franka_description 301-redirects to frankarobotics/franka_description; the
# canonical name is what the fetch NAME, the license annotation and the package-root copy carry.
meios_declare_resource(
    NAME franka_description
    URL  https://github.com/frankarobotics/franka_description/archive/refs/tags/2.8.1.tar.gz
    HASH SHA256=4adcc45f83fd05a5d3d422e1a067fc97bb1d7e28033217debcbe7daba4ab1fd0
    STRIP_TOP_LEVEL
    OUT_DIR MEIOS_CORPUS_FRANKA_DIR)  # franka_description 2.8.1
meios_corpus_check_license(franka_description "${MEIOS_CORPUS_FRANKA_DIR}/package.xml"
    "${_corpus_self}" "Apache 2.0")

# The resolver's containment guard rejects a package root reached through a symlink, so
# every self-contained fetch is copied under the name it resolves itself by. The copy is
# scratch belonging to whichever build tree is running, which is why it is anchored at the
# top of the build rather than under meios's own binary directory. Four packages anchor
# here: ur_description, lbr_med14_r820_description, franka_description, and kuka_experimental
# — whose own kr6r900sixx document keeps resolving against the raw fetch directory, unchanged.
set(MEIOS_CORPUS_PACKAGE_ROOT "${CMAKE_BINARY_DIR}/_meios_corpus_packages")
file(COPY "${MEIOS_CORPUS_UR_DIR}/" DESTINATION "${MEIOS_CORPUS_PACKAGE_ROOT}/ur_description")
file(COPY "${MEIOS_CORPUS_LBR_DIR}/"
     DESTINATION "${MEIOS_CORPUS_PACKAGE_ROOT}/lbr_med14_r820_description")
file(COPY "${MEIOS_CORPUS_FRANKA_DIR}/"
     DESTINATION "${MEIOS_CORPUS_PACKAGE_ROOT}/franka_description")
file(COPY "${MEIOS_CORPUS_KUKA_DIR}/" DESTINATION "${MEIOS_CORPUS_PACKAGE_ROOT}/kuka_experimental")

include("${CMAKE_CURRENT_LIST_DIR}/corpus_documents.cmake")
