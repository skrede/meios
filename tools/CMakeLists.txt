include(FetchContent)

# CLI11 is strictly tool-scoped: no lib/ target links it, and a compile-fail guard
# proves no library header pulls a CLI11 type. Pinned to an immutable commit (never
# a mutable tag) so the fetched source is reproducible; SYSTEM so its headers escape
# the curated -Werror set.
set(CLI11_BUILD_TESTS OFF)
set(CLI11_BUILD_EXAMPLES OFF)
FetchContent_Declare(
    CLI11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG 37bb6edc5317e99af72ef48405e65d9ca5218861  # CLI11 v2.6.2
    SYSTEM
    FIND_PACKAGE_ARGS 2.6 NAMES CLI11
)
FetchContent_MakeAvailable(CLI11)

add_subdirectory(meios)
