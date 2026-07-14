#ifndef HPP_GUARD_MEIOS_IO_BUNDLE_SOURCE_H
#define HPP_GUARD_MEIOS_IO_BUNDLE_SOURCE_H

#include "meios/io/package_source.h"
#include "meios/io/resolved_asset.h"

#include "meios/diagnostic/log_sink.h"

#include <functional>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios
{

// Resolves a folder-bundle layout rooted at a single directory, returning a real
// filesystem path (so it satisfies provides_path). Like directory_source it
// contains resolution to the root and rejects any traversal escape.
class bundle_source
{
public:
    bundle_source(std::filesystem::path root, log_sink &log);

    capability_descriptor capabilities() const;

    std::optional<resolved_asset> locate(std::string_view package, std::string_view relative);

    std::optional<std::filesystem::path> path_of(std::string_view package,
                                                 std::string_view relative) const;

private:
    std::filesystem::path m_root;
    std::reference_wrapper<log_sink> m_log;
};

}

#endif
