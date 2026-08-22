#ifndef HPP_GUARD_MEIOS_IO_PACKAGE_SOURCE_H
#define HPP_GUARD_MEIOS_IO_PACKAGE_SOURCE_H

#include "meios/io/source_lookup.h"
#include "meios/io/resolved_asset.h"

#include <string>
#include <vector>
#include <concepts>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios
{

enum class source_kind
{
    memory,
    directory,
    bundle,
};

constexpr std::string_view to_string(source_kind kind) noexcept
{
    switch(kind)
    {
        case source_kind::memory:
            return "memory";
        case source_kind::directory:
            return "directory";
        case source_kind::bundle:
            return "bundle";
    }
    return "unknown";
}

struct capability_descriptor
{
    source_kind kind;
    bool package_enumerable;
};

template<typename S>
concept package_source = requires(S s, std::string_view pkg, std::string_view rel) {
    { s.capabilities() } -> std::convertible_to<capability_descriptor>;
    { s.locate(pkg, rel) } -> std::convertible_to<std::optional<resolved_asset>>;
};

template<typename S>
concept provides_typed_lookup = package_source<S> && requires(S s, std::string_view pkg, std::string_view rel) {
    { s.try_locate(pkg, rel) } -> std::convertible_to<source_lookup_result>;
};

template<typename S>
concept provides_path = package_source<S> && requires(const S s, std::string_view pkg, std::string_view rel) {
    { s.path_of(pkg, rel) } -> std::convertible_to<std::optional<std::filesystem::path>>;
};

template<typename S>
concept enumerates_packages = package_source<S> && requires(const S s) {
    { s.packages() } -> std::convertible_to<std::vector<std::string>>;
};

}

#endif
