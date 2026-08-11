include(FetchContent)

# One recorded fact per acquired dependency, in a global property rather than a scoped variable
# because the decision that reads them back is taken in a directory that ran none of these sites.
function(_meios_record_dependency_install name option source_dir)
    set(_fetched FALSE)
    set(_installs FALSE)
    if(source_dir)
        set(_fetched TRUE)
    endif()
    if(${option})
        set(_installs TRUE)
    endif()
    set_property(GLOBAL APPEND PROPERTY MEIOS_DEPENDENCY_INSTALL_FACTS
        "${name}|${option}|${_fetched}|${_installs}")
endfunction()

# Each dependency is declared once with FIND_PACKAGE_ARGS, so a single declaration
# expresses both acquisition paths (the top-level FETCHCONTENT_TRY_FIND_PACKAGE_MODE
# picks which one). A found pugixml keeps meios::core's exported target an imported
# reference; a fetched (non-imported) pugixml cannot join install(EXPORT) on the
# $<LINK_ONLY:> edge, which is why a fetched dependency that declines to install
# itself disables meios's own install below.
# GLOBAL on every declaration: without it a found dependency arrives as a directory-scoped
# imported target, invisible to anything outside this directory, which is where a consumer's
# own targets live.
set(PUGIXML_BUILD_TESTS OFF)
# pugixml installs itself by default and meios declines on the consumer's behalf, so a fetched
# dependency is not deployed into a prefix nobody asked to receive it. A consumer that does want
# it says so before adding meios, which is also what makes meios's own export legal when pugixml
# was fetched rather than found.
if(NOT DEFINED PUGIXML_INSTALL)
    set(PUGIXML_INSTALL OFF)
endif()
FetchContent_Declare(
    pugixml
    GIT_REPOSITORY https://github.com/zeux/pugixml.git
    GIT_TAG c8033ce9d039e7f9d134877c363397b3cfe20816  # pugixml v1.16
    SYSTEM
    FIND_PACKAGE_ARGS 1.16 NAMES pugixml GLOBAL
)
FetchContent_MakeAvailable(pugixml)
_meios_record_dependency_install(pugixml PUGIXML_INSTALL "${pugixml_SOURCE_DIR}")

# Opt-in enrichment backends: pinned to immutable refs now so every dependency is
# recorded as FetchContent-able, but only made available when their target builds.
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG 65ee68451d8eb2b5f3a30b410476ab83deb3289b  # nlohmann_json v3.12.0
    SYSTEM
    FIND_PACKAGE_ARGS 3.12 NAMES nlohmann_json GLOBAL
)
FetchContent_Declare(
    miniz
    GIT_REPOSITORY https://github.com/richgel999/miniz.git
    GIT_TAG 77d0dce8627735138c51770d1799a1ef48f2117d  # miniz 3.1.2
    SYSTEM
    FIND_PACKAGE_ARGS NAMES miniz GLOBAL
)
FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG c7fb32eea8c92bebeea9f0735041a72aa20c75f5  # pybind11 v3.0.4
    SYSTEM
    FIND_PACKAGE_ARGS NAMES pybind11 GLOBAL
)
FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG 56e3bb550c91fd7005566f19c079cb7a503223cf  # yaml-cpp 0.9.0
    SYSTEM
    FIND_PACKAGE_ARGS 0.9 NAMES yaml-cpp GLOBAL
)
if(MEIOS_SCAN_GLTF_SUPPORT)
    set(JSON_BuildTests OFF)
    FetchContent_MakeAvailable(nlohmann_json)
    _meios_record_dependency_install(nlohmann_json JSON_Install "${nlohmann_json_SOURCE_DIR}")
endif()
if(MEIOS_ARCHIVE_ZIP_SUPPORT)
    FetchContent_MakeAvailable(miniz)
    # A found miniz arrives namespaced from its own config package and the fetched project
    # declares no alias at all, so one spelling serves both paths. Without it a link site is
    # correct on one acquisition path and a bare library name on the other, and a bare name that
    # resolves to nothing reaches the linker as a raw flag instead of failing to generate.
    if(NOT TARGET miniz::miniz)
        add_library(miniz::miniz ALIAS miniz)
    endif()
    _meios_record_dependency_install(miniz INSTALL_PROJECT "${miniz_SOURCE_DIR}")
endif()
if(MEIOS_YAML_SUPPORT)
    set(YAML_CPP_BUILD_TESTS OFF)
    set(YAML_CPP_BUILD_TOOLS OFF)
    set(YAML_CPP_BUILD_CONTRIB OFF)
    # The inversion against pugixml above is deliberate. yaml-cpp's own default is off whenever it
    # is consumed as a subproject, and the accounting below reads a fetched dependency that
    # declines to install as a reason to disable meios's own install; leaving the default in place
    # would therefore switch the install off for every default build, because this module is the
    # one enrichment that is on by default. A consumer that has already expressed a preference
    # keeps it.
    # Tracking meios's own toggle rather than a bare ON: a tree meios does not install into has no
    # prefix for a dependency to claim either, and a dependency that registers install rules there
    # would install an archive such a tree never built.
    if(NOT DEFINED YAML_CPP_INSTALL)
        set(YAML_CPP_INSTALL ${MEIOS_INSTALL})
    endif()
    FetchContent_MakeAvailable(yaml-cpp)
    if(NOT TARGET yaml-cpp::yaml-cpp)
        add_library(yaml-cpp::yaml-cpp ALIAS yaml-cpp)
    endif()
    _meios_record_dependency_install(yaml-cpp YAML_CPP_INSTALL "${yaml-cpp_SOURCE_DIR}")
endif()
# No install fact is recorded for the interpreter binding: the module that links it withholds
# itself from the export, so nothing in the export set reaches it and recording it would degrade
# every other module's install because of one module's link edge.
if(MEIOS_EVAL_PYTHON_SUPPORT)
    FetchContent_MakeAvailable(pybind11)
endif()

# Read once here, the last point at which the decision can still reach the module helper. A fetched
# dependency that does not install itself is a real, non-imported target belonging to no export
# set, so meios's own export would fail to generate around it.
set(_meios_declining "")
if(MEIOS_INSTALL)
    get_property(_meios_facts GLOBAL PROPERTY MEIOS_DEPENDENCY_INSTALL_FACTS)
    foreach(_fact IN LISTS _meios_facts)
        string(REPLACE "|" ";" _field "${_fact}")
        list(GET _field 0 _name)
        list(GET _field 1 _option)
        list(GET _field 2 _fetched)
        list(GET _field 3 _installs)
        if(_fetched AND NOT _installs)
            list(APPEND _meios_declining "${_name} (turn ${_option} on)")
        endif()
    endforeach()
endif()
if(_meios_declining)
    string(REPLACE ";" ", " _meios_declining "${_meios_declining}")
    message(STATUS
        "meios: not installing, because a fetched dependency declines to install itself and so "
        "belongs to no export set: ${_meios_declining}. Nothing meios builds reaches your prefix, "
        "including meios's CMake resource modules. Supply a discoverable version of it instead, "
        "or turn the named option on before adding meios.")
    # Under a consumer this reaches meios's own directory scope and stops there; a variable of the
    # same name in the consumer's scope is untouched, which is what it should be.
    set(MEIOS_INSTALL OFF)
    set(MEIOS_INSTALL OFF PARENT_SCOPE)
endif()
