# A bounded, portable warning family shared by every first-party compiled target.
# Kept deliberately narrow (not -Weverything) so a newer toolchain's fresh
# diagnostic cannot silently turn a green build red for an adopter.
set(MEIOS_WARNING_FLAGS
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive- /utf-8>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:
        -fPIC -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
        -Wold-style-cast -Wcast-align -Woverloaded-virtual -Wnon-virtual-dtor
        -Wdouble-promotion -Wimplicit-fallthrough -Wformat=2>
)

# The only switch that injects -Werror. Default is meios_IS_TOP_LEVEL: ON for
# meios's own build, OFF automatically for any project consuming meios through
# add_subdirectory()/FetchContent, so meios's warning drift can never break a
# consumer's build.
option(MEIOS_WERROR "Treat warnings as errors for first-party targets" ${meios_IS_TOP_LEVEL})

# Applied per target, never directory-globally: a directory-scope option would
# leak the curated set into the SYSTEM-consumed third-party builds, whose code is
# not ours to lint.
function(meios_warnings target)
    target_compile_options(${target} PRIVATE ${MEIOS_WARNING_FLAGS})
    if(MSVC)
        # Removes only the CRT-secure C4996 subfamily, so C4996 still fires for
        # genuine [[deprecated]] APIs.
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
    if(MEIOS_WERROR)
        if(MSVC)
            target_compile_options(${target} PRIVATE /WX)
        else()
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

function(meios_enable_coverage target)
    if(NOT MEIOS_COVERAGE OR NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        return()
    endif()
    target_compile_options(${target} PRIVATE
        --coverage -fprofile-arcs -ftest-coverage -O0 -g)
    # BUILD_INTERFACE so the instrumentation never leaks into the installed export.
    target_link_options(${target} PUBLIC $<BUILD_INTERFACE:--coverage>)
endfunction()
