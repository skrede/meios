#ifndef HPP_GUARD_MEIOS_XACRO_INCLUDE_LOCATE_H
#define HPP_GUARD_MEIOS_XACRO_INCLUDE_LOCATE_H

#include "meios/io/resolved_asset.h"

#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/operation_failure.h"

#include <span>
#include <string>
#include <cstddef>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios
{
class source_stack;
}

namespace meios::detail
{

struct beside_probe
{
    std::optional<resolved_asset> hit;
    std::optional<operation_failure> cause;
    bool uncontained;
};

// The layer is a rank in the expansion's source stack, which no step of the expansion reorders.
struct served_document
{
    std::size_t layer;
    std::string package;
    std::string relative;
};

struct served_hit
{
    resolved_asset asset;
    std::size_t layer;
};

struct located_include
{
    resolved_asset asset;
    std::filesystem::path named;
    std::optional<served_document> served;
};

beside_probe probe_beside(const std::filesystem::path &document, const std::string &spelling, std::span<const std::filesystem::path> roots);

beside_probe probe_unbounded(const std::filesystem::path &candidate);

// The stack asks its layers in rank order and keeps the first answer, so the first to answer served
// it; each is asked once, since a layer reports through its own sink and a repeat repeats its records.
std::optional<served_hit> locate_served(source_stack &sources, std::string_view package, std::string_view relative, log_sink &log);

served_document sibling_of(const served_document &served, const std::string &normalized);

std::optional<resolved_asset> probe_served(source_stack &sources, const served_document &sibling);

// A layer that names no path for the document it served holds it only as the bytes it materialized.
bool serves_bytes(source_stack &sources, const served_document &served);

// Empty when the spelling, joined onto the served document's directory, leaves its package.
std::optional<served_document> climbed_sibling(const served_document &served, const std::string &written);

}

#endif
