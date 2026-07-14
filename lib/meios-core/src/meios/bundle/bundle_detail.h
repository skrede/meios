#ifndef HPP_GUARD_MEIOS_CORE_BUNDLE_BUNDLE_DETAIL_H
#define HPP_GUARD_MEIOS_CORE_BUNDLE_BUNDLE_DETAIL_H

#include "meios/diagnostic/log_sink.h"

#include <string>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios::detail
{

struct parsed_reference
{
    std::string pkg;
    std::string rel;
    bool is_texture;
};

// Parses the ORIGINAL reference (never the resolved absolute path): package://pkg/rel
// splits on the first slash, a plain relative path buckets under the bundle's own
// synthetic package, and a malformed package:// (no slash) loud-fails with nullopt.
std::optional<parsed_reference> parse_reference(std::string_view original, bool is_texture,
                                                std::string_view bundle_name, log_sink &log);

std::string layout_dest(bool is_texture, std::string_view pkg_dir, std::string_view rel);

std::string bundle_uri(std::string_view bundle_name, std::string_view dest);

std::string extension_of(const std::filesystem::path &source);

std::filesystem::path source_root_of(const std::filesystem::path &resolved, std::string_view rel);

// weakly_canonical generic-string form so the same logical root hashes identically
// regardless of separator or ./.. noise and across platforms.
std::string normal_root(const std::filesystem::path &root);

std::string collision_suffix(const std::filesystem::path &root);

}

#endif
