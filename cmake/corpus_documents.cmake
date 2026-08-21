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

# The only family here whose descriptions read a loaded mapping by a dotted member name, and the
# only one that carries such a mapping through a macro parameter. All eight top-level documents it
# ships were rendered against the upstream this project measures and their renders compared byte
# for byte: no two are alike, so none of the six below stands in for another and each carries its
# own measurement. Every declared argument of each defaults, so none takes an override.
#
# The two absent ones are a finding rather than an omission. fr3_duo and mobile_fr3_duo_v0_2 build
# their arm list with a bracket literal and then take a slice of it; neither spelling is admitted
# here, so both refuse and a corpus document that does not load would claim coverage it has none of.
#
# Nothing under robots/common, accessories or end_effectors is a document -- those are the macro
# fragments the rule at the top of this file describes. Neither is a .srdf.xacro: a semantic
# description names links and joints that are declared elsewhere, so a measurement taken from one
# would record references rather than a model.
meios_corpus_document("${MEIOS_CORPUS_PACKAGE_ROOT}/franka_description/robots/fer/fer.urdf.xacro"
    "" native "${MEIOS_CORPUS_PACKAGE_ROOT}")
meios_corpus_document("${MEIOS_CORPUS_PACKAGE_ROOT}/franka_description/robots/fp3/fp3.urdf.xacro"
    "" native "${MEIOS_CORPUS_PACKAGE_ROOT}")
meios_corpus_document("${MEIOS_CORPUS_PACKAGE_ROOT}/franka_description/robots/fr3/fr3.urdf.xacro"
    "" native "${MEIOS_CORPUS_PACKAGE_ROOT}")
meios_corpus_document(
    "${MEIOS_CORPUS_PACKAGE_ROOT}/franka_description/robots/fr3v2/fr3v2.urdf.xacro"
    "" native "${MEIOS_CORPUS_PACKAGE_ROOT}")
meios_corpus_document(
    "${MEIOS_CORPUS_PACKAGE_ROOT}/franka_description/robots/fr3v2_1/fr3v2_1.urdf.xacro"
    "" native "${MEIOS_CORPUS_PACKAGE_ROOT}")
meios_corpus_document(
    "${MEIOS_CORPUS_PACKAGE_ROOT}/franka_description/robots/tmrv0_2/tmrv0_2.urdf.xacro"
    "" native "${MEIOS_CORPUS_PACKAGE_ROOT}")

# Multi-package, so the fetch directory is its own package root, as kr6r900sixx above already is.
# The dof argument selects the seven-degree arm; gripper defaults empty, and the two references to
# an out-of-repository package sit behind <xacro:unless value="${not gripper}">, which that default
# does not open -- a load reaching one would refuse, since no such package is in the tarball.
meios_corpus_document("${MEIOS_CORPUS_KORTEX_DIR}/kortex_description/robots/gen3.xacro"
    "dof=7" native "${MEIOS_CORPUS_KORTEX_DIR}")

# A second top-level document of the Universal Robots package above, driven with a distinguishing
# name because ur_type alone would key it identically to the plain ur.urdf.xacro entry point and a
# shared key pairs one document with another's measured facts. The name is also the rendered robot
# name. It reaches the keyword-argument mapping constructor through a macro-parameter default its
# own call overrides, and declares a property fallback where ur.urdf.xacro declares a value.
meios_corpus_document("${MEIOS_CORPUS_PACKAGE_ROOT}/ur_description/urdf/ur_mocked.urdf.xacro"
    "name=ur_mocked ur_type=ur5e" native "${MEIOS_CORPUS_PACKAGE_ROOT}")

# The one entry point here this project wrote rather than pinned. Its configuration is loaded by
# name and resolved relative to the document loading it, so the pair is self-contained and its
# record names no package root -- the only record here that does not.
meios_corpus_document("${MEIOS_CORPUS_MERGED_DIR}/gantry.urdf.xacro" "" native "")

list(LENGTH MEIOS_CORPUS_DOCUMENTS _corpus_fields)
math(EXPR MEIOS_CORPUS_DOCUMENT_COUNT "${_corpus_fields} / 4")
