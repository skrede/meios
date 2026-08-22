include_guard(GLOBAL)

# corpus.cmake pairs every fetch with a "# license[<name>]:" line; this gate checks two
# facts about each one. The annotation's leading token is the canonical identifier — the
# rest of the line is a parenthetical provenance note the extraction stops at — and the
# call site states, separately, the exact free text the upstream's own manifest declares.
# The two differ in general: an upstream may write "Apache 2.0" for the identifier
# "Apache-2.0", so the declaration cannot be derived from the identifier. A manifest that
# stops matching its stated declaration between pinned tags FATALs naming the entry and
# both values, rather than shipping an unverified claim into CI.
function(_meios_corpus_manifest_license name manifest out)
    if(NOT EXISTS "${manifest}")
        message(FATAL_ERROR
            "corpus: fetch entry '${name}' names a manifest at '${manifest}' that does not exist.")
    endif()
    file(READ "${manifest}" _manifest_text)
    string(REGEX MATCH "<license[^>]*>([^<]*)</license>" _found "${_manifest_text}")
    if(NOT _found)
        message(FATAL_ERROR
            "corpus: fetch entry '${name}''s manifest '${manifest}' carries no <license> element.")
    endif()
    string(STRIP "${CMAKE_MATCH_1}" _declared)
    set(${out} "${_declared}" PARENT_SCOPE)
endfunction()

function(_meios_corpus_recorded_license name self_text out)
    string(REGEX MATCH "# license\\[${name}\\]:[ \t]*([^ \t\r\n(]+)" _recorded "${self_text}")
    if(NOT _recorded OR "${CMAKE_MATCH_1}" STREQUAL "")
        message(FATAL_ERROR
            "corpus: fetch entry '${name}' has no '# license[${name}]:' determination to check.")
    endif()
    set(${out} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

function(meios_corpus_check_license name manifest self_text declared)
    _meios_corpus_recorded_license("${name}" "${self_text}" _identifier)
    _meios_corpus_manifest_license("${name}" "${manifest}" _manifest_license)
    if(NOT _manifest_license STREQUAL "${declared}")
        message(FATAL_ERROR
            "corpus: fetch entry '${name}' records license '${_identifier}' and states its "
            "manifest declares '${declared}', but '${manifest}' declares '${_manifest_license}'.")
    endif()
endfunction()
