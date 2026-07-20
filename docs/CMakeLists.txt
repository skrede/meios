# Documentation snippet compile gate.
#
# The docs and the README carry HTML-comment sentinels marking which fenced
# ``cpp`` blocks are real, buildable code. tools/extract_doc_snippets.py turns each
# marked block into a standalone translation unit; those units are compiled here
# against the actual public headers via the build-tree meios::urdf target. A
# renamed symbol or a wrong signature in the prose therefore breaks the build
# instead of rotting silently.
#
# This target is compile-only: it is never linked into an executable and never
# run. Its sole purpose is to force the compiler across the documented API. It
# links the build-tree meios::urdf alias -- never meios::core, never the installed
# package -- so it exercises the full consumer include surface a newcomer reaches
# for and fails on the exact FetchContent-tier defect an installed-package gate
# would stay green on.

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(_doc_snippet_extractor ${CMAKE_SOURCE_DIR}/tools/extract_doc_snippets.py)
set(_doc_snippet_gen_dir ${CMAKE_CURRENT_BINARY_DIR}/doc_snippets)
set(_doc_snippet_readme ${CMAKE_SOURCE_DIR}/README.md)

# Regenerate the snippets at configure time over exactly the README and docs/, so
# stray repo markdown is never dragged in. A malformed sentinel makes the
# extractor exit non-zero, which we promote to a hard configure error so the gate
# fails loudly rather than silently dropping a snippet.
execute_process(
    COMMAND ${Python3_EXECUTABLE} ${_doc_snippet_extractor}
            --docs-root ${_doc_snippet_readme}
            --docs-root ${CMAKE_CURRENT_SOURCE_DIR}
            --out ${_doc_snippet_gen_dir}
    RESULT_VARIABLE _doc_snippet_result
    OUTPUT_VARIABLE _doc_snippet_output
    ERROR_VARIABLE _doc_snippet_error)

if (NOT _doc_snippet_result EQUAL 0)
    message(FATAL_ERROR
        "Documentation snippet extraction failed:\n${_doc_snippet_output}${_doc_snippet_error}")
endif ()

message(STATUS "Documentation snippets: ${_doc_snippet_output}")

# Re-run extraction whenever the extractor or any scanned markdown changes, so an
# incremental build after a doc edit picks up the new snippets.
file(GLOB_RECURSE _doc_snippet_markdown CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.md)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${_doc_snippet_extractor} ${_doc_snippet_readme} ${_doc_snippet_markdown})

# The always-compiled core group: snippets with no capability requirement. Include
# roots and the cxx_std_20 feature come entirely from the linked meios::urdf target
# -- there is no hand-rolled -I list anywhere in this file.
file(GLOB _doc_snippet_core CONFIGURE_DEPENDS ${_doc_snippet_gen_dir}/core/*.cpp)

set(_doc_snippet_sources ${_doc_snippet_core})
set(_doc_snippet_link_libs meios::urdf)

# Capability-gated groups. The extractor emits every marked snippet regardless of
# the build configuration; a ``needs=<cap>`` group is compiled only when the
# matching enrichment target exists (guarding on the target, not the option: a
# glob-materialized or embed-dependent enrichment may be absent even with its
# option ON), and reported (skipped) otherwise.
set(_doc_snippet_capabilities
    scan-obj scan-stl scan-collada scan-gltf ros archive-zip eval-python)

foreach (_cap IN LISTS _doc_snippet_capabilities)
    file(GLOB _doc_snippet_group CONFIGURE_DEPENDS ${_doc_snippet_gen_dir}/${_cap}/*.cpp)
    list(LENGTH _doc_snippet_group _doc_snippet_group_count)
    if (_doc_snippet_group_count GREATER 0)
        if (TARGET meios_${_cap})
            list(APPEND _doc_snippet_sources ${_doc_snippet_group})
            list(APPEND _doc_snippet_link_libs meios::${_cap})
            message(STATUS
                "Documentation snippets: compiling ${_doc_snippet_group_count} '${_cap}' snippet(s) (meios::${_cap} present)")
        else ()
            message(STATUS
                "Documentation snippets: skipping ${_doc_snippet_group_count} '${_cap}' snippet(s) (meios::${_cap} absent)")
        endif ()
    endif ()
endforeach ()

list(LENGTH _doc_snippet_sources _doc_snippet_source_count)
if (_doc_snippet_source_count EQUAL 0)
    message(FATAL_ERROR "Documentation snippet gate is enabled but no snippets were extracted.")
endif ()

add_library(doc_snippets OBJECT ${_doc_snippet_sources})
target_link_libraries(doc_snippets PRIVATE ${_doc_snippet_link_libs})
