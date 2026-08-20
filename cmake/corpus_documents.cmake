include_guard(GLOBAL)

# The records half of the corpus. Included from the acquisition half once every fetch
# OUT_DIR and the package-root copies exist, because every path below is built from them.

# Every corpus document is named outright rather than found by globbing an extension.
# A glob over ".urdf" made the tier look broader than it was, and pointing it at the
# macro extension instead loads fragments as documents: twenty-one of them carry a
# <robot> root with no name and no links, so every refusal would be a false one.
# A record is four fields — absolute path, space-separated key=value expansion
# arguments, the evaluator the document needs, and a package root or nothing — and
# the records are joined with the "|" the resource paths already travel under.
# The evaluator vocabulary is closed at three spellings, of which the corpus names two:
# core for a document that needs no expression evaluation, and native for one the
# built-in evaluator must handle alone. The third selects the CPython backend and no
# record here needs it. Any other spelling fails the document rather than falling
# through to a default backend.
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

# The only corpus document resolving $(find) across two sibling packages. The fetched
# tarball root already holds both under the names the description resolves them by, so
# it serves as the package root directly and no copy is needed.
meios_corpus_document("${MEIOS_CORPUS_KUKA_DIR}/kuka_kr6_support/urdf/kr6r900sixx.xacro"
    "" native "${MEIOS_CORPUS_KUKA_DIR}")

# The shipped ur_type default is deliberately invalid, so a variant must be named. ur3e's
# third wrist has no position limits, so a boolean read out of the auxiliary document drives
# the branch that types the joint continuous, flowing through a string comparison into a
# limit element carrying no position keys — a path ur5e cannot reach. safety_limits guards
# one <safety_controller> element per joint and force_abs_paths changes only mesh-URI
# spelling, so each branch keeps every row plain ur5e's record already covers.
meios_corpus_document("${MEIOS_CORPUS_PACKAGE_ROOT}/ur_description/urdf/ur.urdf.xacro"
    "ur_type=ur5e" native "${MEIOS_CORPUS_PACKAGE_ROOT}")
meios_corpus_document("${MEIOS_CORPUS_PACKAGE_ROOT}/ur_description/urdf/ur.urdf.xacro"
    "ur_type=ur3e" native "${MEIOS_CORPUS_PACKAGE_ROOT}")
meios_corpus_document("${MEIOS_CORPUS_PACKAGE_ROOT}/ur_description/urdf/ur.urdf.xacro"
    "ur_type=ur7e" native "${MEIOS_CORPUS_PACKAGE_ROOT}")
meios_corpus_document("${MEIOS_CORPUS_PACKAGE_ROOT}/ur_description/urdf/ur.urdf.xacro"
    "ur_type=ur5e safety_limits=true" native "${MEIOS_CORPUS_PACKAGE_ROOT}")
meios_corpus_document("${MEIOS_CORPUS_PACKAGE_ROOT}/ur_description/urdf/ur.urdf.xacro"
    "ur_type=ur5e force_abs_paths=true" native "${MEIOS_CORPUS_PACKAGE_ROOT}")

# Both of its xacro:arg declarations default, so the entry point loads with no key=value
# override at all.
meios_corpus_document(
    "${MEIOS_CORPUS_PACKAGE_ROOT}/lbr_med14_r820_description/urdf/lbr_med14_r820.urdf.xacro"
    "" native "${MEIOS_CORPUS_PACKAGE_ROOT}")

list(LENGTH MEIOS_CORPUS_DOCUMENTS _corpus_fields)
math(EXPR MEIOS_CORPUS_DOCUMENT_COUNT "${_corpus_fields} / 4")
