include_guard(GLOBAL)

# Extends corpus.cmake's self-check: where a fetch entry's own fetched manifest is
# available, its <license> element is read and cross-checked against the recorded
# "# license[<name>]:" comment's leading token — the comment carries a parenthetical
# provenance note after the SPDX identifier, so only the first word is the checked
# claim. A missing manifest, an unreadable element, or a value that disagrees FATALs
# naming the entry and both values, so an unlicensed or misdeclared fetch fails
# configure rather than shipping an unverified claim.
function(meios_corpus_check_license name manifest self_text)
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
    string(STRIP "${CMAKE_MATCH_1}" _manifest_license)
    string(REGEX MATCH "# license\\[${name}\\]:[ \t]*([^ \t\r\n(]+)" _recorded "${self_text}")
    if(NOT _recorded)
        message(FATAL_ERROR
            "corpus: fetch entry '${name}' has no '# license[${name}]:' determination to check.")
    endif()
    if(NOT _manifest_license STREQUAL "${CMAKE_MATCH_1}")
        message(FATAL_ERROR
            "corpus: fetch entry '${name}' records license '${CMAKE_MATCH_1}', but its manifest "
            "'${manifest}' declares '${_manifest_license}'.")
    endif()
endfunction()
