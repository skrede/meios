#include "meios/ros/ros_package_source.h"

#include "meios/io/directory_source.h"

#include <utility>

namespace meios
{

ros_package_source::ros_package_source(std::vector<std::filesystem::path> ros1_roots,
                                       std::vector<std::filesystem::path> ament_prefixes,
                                       log_sink &log)
    : m_bases(detail::discover_packages(ros1_roots, ament_prefixes, log)), m_log(log)
{
}

capability_descriptor ros_package_source::capabilities() const
{
    return { source_kind::directory, true, true };
}

std::optional<std::filesystem::path> ros_package_source::path_of(std::string_view package,
                                                                 std::string_view relative) const
{
    auto entry = m_bases.find(std::string(package));
    if(entry == m_bases.end())
        return std::nullopt;
    std::optional<std::filesystem::path> candidate =
        detail::contained_candidate(entry->second, "", relative, m_log.get());
    if(!candidate || !std::filesystem::exists(*candidate))
        return std::nullopt;
    return candidate;
}

std::optional<resolved_asset> ros_package_source::locate(std::string_view package,
                                                         std::string_view relative)
{
    std::optional<std::filesystem::path> candidate = path_of(package, relative);
    if(!candidate)
        return std::nullopt;
    return resolved_asset{ std::move(*candidate) };
}

std::vector<std::string> ros_package_source::packages() const
{
    std::vector<std::string> names;
    names.reserve(m_bases.size());
    for(const auto &entry : m_bases)
        names.push_back(entry.first);
    return names;
}

}
