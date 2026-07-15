#include "meios/io/directory_source.h"

#include "meios/diagnostic/level.h"

#include <string>
#include <utility>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios
{

namespace
{

bool escapes_root(const std::filesystem::path &root, const std::filesystem::path &candidate)
{
    std::filesystem::path relative = candidate.lexically_relative(root);
    if(relative.empty())
        return true;
    return *relative.begin() == std::filesystem::path("..");
}

std::string reject_message(std::string_view package, std::string_view relative)
{
    return "rejected package path \"" + std::string(package) + '/' + std::string(relative)
         + "\" that escapes the configured source root";
}

}

std::optional<std::filesystem::path> detail::contained_candidate(
    const std::filesystem::path &root, std::string_view package,
    std::string_view relative, log_sink &log)
{
    // Containment is judged on the logical path, not the symlink-resolved real
    // path: a colcon --symlink-install package dir is an in-root symlink to a
    // source tree outside the root, so weakly_canonical would false-positive an
    // escape. lexically_normal collapses `..` without following links, keeping the
    // first-component `..` check as the real parent-escape guard.
    std::filesystem::path base = root.lexically_normal();
    std::filesystem::path candidate =
        (root / std::string(package) / std::string(relative)).lexically_normal();
    // An empty relative leaves a trailing separator that lexically_normal keeps but
    // weakly_canonical dropped; strip it so $(find pkg) yields the bare base path.
    if(!candidate.has_filename() && candidate != candidate.root_path())
        candidate = candidate.parent_path();
    if(escapes_root(base, candidate))
    {
        log.log(level::error, reject_message(package, relative));
        return std::nullopt;
    }
    return candidate;
}

directory_source::directory_source(std::filesystem::path root, log_sink &log)
    : m_root(std::move(root)), m_log(log)
{
}

capability_descriptor directory_source::capabilities() const
{
    return { source_kind::directory, true, false };
}

std::optional<resolved_asset> directory_source::locate(std::string_view package,
                                                       std::string_view relative)
{
    std::optional<std::filesystem::path> candidate =
        detail::contained_candidate(m_root, package, relative, m_log.get());
    if(!candidate || !std::filesystem::exists(*candidate))
        return std::nullopt;
    return resolved_asset{ *candidate };
}

std::optional<std::filesystem::path> directory_source::path_of(std::string_view package,
                                                               std::string_view relative) const
{
    std::optional<std::filesystem::path> candidate =
        detail::contained_candidate(m_root, package, relative, m_log.get());
    if(!candidate || !std::filesystem::exists(*candidate))
        return std::nullopt;
    return candidate;
}

}
