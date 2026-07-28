#ifndef HPP_GUARD_MEIOS_URDF_ASSET_URI_H
#define HPP_GUARD_MEIOS_URDF_ASSET_URI_H

#include "meios/urdf/parse_context.h"

#include "meios/diagnostic/source_location.h"

#include <string>
#include <optional>
#include <string_view>

namespace meios::detail
{

enum class asset_uri_form
{
    package,
    relative,
    absolute,
    foreign_scheme,
};

// Answers the scheme text before "://", or empty when the URI carries no scheme.
std::optional<std::string_view> scheme_of(std::string_view uri);

asset_uri_form classify_asset_uri(std::string_view uri);

// Answers the authority a file:// URI carries, empty for the authority-less spelling and for
// the lenient drive spelling, which is a path rather than a host.
std::string_view file_authority(std::string_view uri);

// Strips the scheme and percent-decodes, so a file:// URI becomes the plain absolute path it
// names and from there travels the identical code path a bare absolute path does.
std::string file_uri_to_path(std::string_view uri);

// Answers the path an asset URI names, or empty once it has said why there is none. Every
// form a description can author reaches it, and none of them leaves without either a path
// or a diagnostic.
std::optional<std::string> resolve_asset_uri(const std::string &uri, parse_context &ctx,
                                             const source_location &loc);

}

#endif
