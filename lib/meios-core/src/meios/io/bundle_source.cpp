#include "meios/io/bundle_source.h"
#include "meios/io/directory_source.h"

#include <utility>
#include <optional>
#include <filesystem>
#include <string_view>
#include <system_error>

namespace meios
{

bundle_source::bundle_source(std::filesystem::path root, log_sink &log)
    : m_root(std::move(root)), m_log(log)
{
}

capability_descriptor bundle_source::capabilities() const
{
    return { source_kind::bundle, false };
}

std::optional<resolved_asset> bundle_source::locate(std::string_view package,
                                                    std::string_view relative)
{
    std::optional<std::filesystem::path> candidate =
        detail::contained_candidate(m_root, package, relative, m_log.get());
    std::error_code ec;
    if(!candidate || !std::filesystem::exists(*candidate, ec))
        return std::nullopt;
    return resolved_asset{ *candidate };
}

std::optional<std::filesystem::path> bundle_source::path_of(std::string_view package,
                                                            std::string_view relative) const
{
    std::optional<std::filesystem::path> candidate =
        detail::contained_candidate(m_root, package, relative, m_log.get());
    std::error_code ec;
    if(!candidate || !std::filesystem::exists(*candidate, ec))
        return std::nullopt;
    return candidate;
}

}
