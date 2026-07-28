include_guard(GLOBAL)

# Pinned, license-recorded robot-description corpus, fetched as DATA through
# meios_declare_resource (parsed, never compiled, never linked). The always-on
# tier fetches one small pinned known-good description plus an in-repo crafted
# known-faulty fixture wired via SOURCE_DIR; the breadth tier adds the remaining
# top-level documents that same upstream ships.

option(MEIOS_FETCH_CORPUS
    "Fetch the pinned robot-description corpus and build the corpus test" OFF)
option(MEIOS_CORPUS_BREADTH
    "Load every top-level document the pinned known-good upstream ships" OFF)
option(MEIOS_CORPUS_EXPRESSION_DOCUMENTS
    "Fetch the expression-valued upstream and load it; needs meios::eval-python" OFF)

# License determinations for every fetched upstream. Each corpus fetch NAME must
# carry a matching "# license[<name>]:" line below; the check further down reads
# this very file, extracts every meios_declare_resource NAME, and FATALs if any
# lacks its annotation — so a future fetch added without a recorded license fails
# configure rather than shipping an undetermined license into CI.
#
# # license[kuka_experimental]: Apache-2.0 (ros-industrial/kuka_experimental, root LICENSE)
# # license[ur_description]: BSD-3-Clause (UniversalRobots/Universal_Robots_ROS2_Description, root LICENSE)
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
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/tests/fixtures/urdf/corpus_faulty"
    OUT_DIR MEIOS_CORPUS_FAULTY_DIR)

set(MEIOS_CORPUS_FAULTY "${MEIOS_CORPUS_FAULTY_DIR}/faulty.urdf")
# faulty.urdf: <robot> on line 2, root_b (the additional root) declared on line 4.
set(MEIOS_CORPUS_FAULTY_LINE 4)

# Every corpus document is named outright rather than found by globbing an extension.
# A glob over ".urdf" made the tier look broader than it was, and pointing it at the
# macro extension instead loads fragments as documents: twenty-one of them carry a
# <robot> root with no name and no links, so every refusal would be a false one.
# A record is four fields — absolute path, space-separated key=value expansion
# arguments, the evaluator the document needs, and a package root or nothing — and
# the records are joined with the "|" the resource paths already travel under.
set(MEIOS_CORPUS_DOCUMENTS "")
macro(meios_corpus_document _path _args _eval _root)
    list(APPEND MEIOS_CORPUS_DOCUMENTS "${_path}" "${_args}" "${_eval}" "${_root}")
endmacro()

if(MEIOS_CORPUS_BREADTH)
    foreach(_doc IN ITEMS
            kuka_kr120_support/urdf/kr120r2500pro.urdf
            kuka_kr16_support/urdf/kr16_2.urdf
            kuka_kr210_support/urdf/kr210l150.urdf
            kuka_lbr_iiwa_support/urdf/lbr_iiwa_14_r820.urdf)
        meios_corpus_document("${MEIOS_CORPUS_KUKA_DIR}/${_doc}" "" core "")
    endforeach()
endif()

if(MEIOS_CORPUS_EXPRESSION_DOCUMENTS)
    meios_declare_resource(
        NAME ur_description
        URL  https://github.com/UniversalRobots/Universal_Robots_ROS2_Description/archive/refs/tags/4.3.1.tar.gz
        HASH SHA256=3532a25c9942bedcdfe41c845be6d144bbbbcd76f179f14a53a3e7b6972ae72d
        STRIP_TOP_LEVEL
        OUT_DIR MEIOS_CORPUS_UR_DIR)  # ur_description 4.3.1

    # The resolver's containment guard rejects a package root reached through a
    # symlink, so the tree is copied under the name the description resolves it by.
    set(MEIOS_CORPUS_PACKAGE_ROOT "${CMAKE_BINARY_DIR}/_meios_corpus_packages")
    file(COPY "${MEIOS_CORPUS_UR_DIR}/" DESTINATION "${MEIOS_CORPUS_PACKAGE_ROOT}/ur_description")

    # The shipped ur_type default is deliberately invalid, so a variant must be named;
    # the macros subscript mapping values, which only the Python evaluator can do.
    meios_corpus_document("${MEIOS_CORPUS_PACKAGE_ROOT}/ur_description/urdf/ur.urdf.xacro"
        "ur_type=ur5e" python "${MEIOS_CORPUS_PACKAGE_ROOT}")
endif()

list(LENGTH MEIOS_CORPUS_DOCUMENTS _corpus_fields)
math(EXPR MEIOS_CORPUS_DOCUMENT_COUNT "${_corpus_fields} / 4")
