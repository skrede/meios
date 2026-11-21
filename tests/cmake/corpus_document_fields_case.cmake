cmake_minimum_required(VERSION 3.28)

# The records are counted and never read, so placeholder directories stand in for the fetched corpus.
set(MEIOS_CORPUS_KUKA_DIR "/corpus/kuka")
set(MEIOS_CORPUS_PACKAGE_ROOT "/corpus/packages")
set(MEIOS_CORPUS_KORTEX_DIR "/corpus/kortex")
set(MEIOS_CORPUS_MERGED_DIR "/corpus/merged")
set(MEIOS_CORPUS_BREADTH OFF)
include("${MEIOS_CMAKE_DIR}/corpus_documents.cmake")

list(LENGTH MEIOS_CORPUS_DOCUMENTS before)
math(EXPR expected "${MEIOS_CORPUS_DOCUMENT_COUNT} * 5")
if(NOT before EQUAL expected)
    message(FATAL_ERROR "corpus document fields: ${before} fields for ${MEIOS_CORPUS_DOCUMENT_COUNT} records")
endif()

meios_corpus_document(/a "" core "" k=1 j=2)
list(LENGTH MEIOS_CORPUS_DOCUMENTS after)
math(EXPR added "${after} - ${before}")
list(GET MEIOS_CORPUS_DOCUMENTS -1 last)
if(NOT added EQUAL 5 OR NOT last STREQUAL "k=1 j=2")
    message(FATAL_ERROR "corpus document fields: a call with two further arguments added ${added} fields, the last '${last}'")
endif()
message(STATUS "MEIOS_CORPUS_DOCUMENT_FIELDS_PASSED records=${MEIOS_CORPUS_DOCUMENT_COUNT}")
