#ifndef HPP_GUARD_MEIOS_IO_DIRECTORY_SOURCE_H
#define HPP_GUARD_MEIOS_IO_DIRECTORY_SOURCE_H

#include "meios/io/source_lookup.h"
#include "meios/io/package_source.h"
#include "meios/io/resolved_asset.h"

#include "meios/expected.h"

#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/operation_failure.h"

#include <functional>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios
{

namespace detail
{

using contained_path_result = expected<std::optional<std::filesystem::path>, operation_failure>;

contained_path_result try_contained_under(const std::filesystem::path &root, const std::filesystem::path &candidate);

// Canonicalizes both and answers the canonicalized candidate only when it stays
// within the root, logging nothing. weakly_canonical resolves symlinks, so an
// in-root link whose real target leaves the root canonicalizes to an out-of-root
// path and is rejected here. Symlink-install workspaces are supported at the ros
// layer, which registers a package name to its real resolved directory before it
// reaches this guard. A filesystem error (e.g. an over-long attacker path) fails
// closed: the candidate is rejected rather than accepted uncanonicalized.
std::optional<std::filesystem::path> contained_under(const std::filesystem::path &root, const std::filesystem::path &candidate);

// Joins root/package/relative and holds it to contained_under; an escaping
// candidate is rejected with a loud typed diagnostic and yields nullopt. Shared by
// the path-backed sources.
std::optional<std::filesystem::path> contained_candidate(const std::filesystem::path &root, std::string_view package, std::string_view relative, log_sink &log);

}

// Resolves package://<pkg>/<rel> against a single configured root directory,
// returning a real filesystem path (so it satisfies provides_path). A traversal
// candidate that escapes the root is rejected, never returned.
class directory_source
{
public:
    directory_source(std::filesystem::path root, log_sink &log);

    static capability_descriptor capabilities();

    std::optional<resolved_asset> locate(std::string_view package, std::string_view relative);

    source_lookup_result try_locate(std::string_view package, std::string_view relative);

    std::optional<std::filesystem::path> path_of(std::string_view package, std::string_view relative) const;

private:
    std::filesystem::path m_root;
    std::reference_wrapper<log_sink> m_log;
};

}

#endif
