#ifndef HPP_GUARD_MEIOS_ROS_ROS_PACKAGE_SOURCE_H
#define HPP_GUARD_MEIOS_ROS_ROS_PACKAGE_SOURCE_H

#include "meios/io/package_source.h"
#include "meios/io/resolved_asset.h"

#include "meios/diagnostic/log_sink.h"

#include <map>
#include <string>
#include <vector>
#include <functional>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios
{

namespace detail
{

// Crawls the ROS1 roots and reads the ROS2 ament indices into one
// package-name -> package-base-directory map. Ament prefixes are consumed first
// and a later duplicate never overwrites an earlier one, so within the map ROS2
// wins over ROS1 and the first prefix/root wins on a collision.
std::map<std::string, std::filesystem::path> discover_packages(
    const std::vector<std::filesystem::path> &ros1_roots,
    const std::vector<std::filesystem::path> &ament_prefixes, log_sink &log);

}

// Resolves package://<pkg>/<rel> across a ROS1 (ROS_PACKAGE_PATH / package.xml)
// and a ROS2 (ament index) workspace with no dependency beyond pugixml. It reports
// source_kind::directory: the frozen enum has no ros member and the resolution is
// plain path-backed directory lookup, so no distinct kind is warranted. Candidate
// paths route through the shared containment guard; an escaping rel is rejected.
class ros_package_source
{
public:
    ros_package_source(std::vector<std::filesystem::path> ros1_roots,
                       std::vector<std::filesystem::path> ament_prefixes, log_sink &log);

    static ros_package_source from_environment(log_sink &log);

    capability_descriptor capabilities() const;

    std::optional<resolved_asset> locate(std::string_view package, std::string_view relative);

    std::optional<std::filesystem::path> path_of(std::string_view package,
                                                 std::string_view relative) const;

    std::vector<std::string> packages() const;

private:
    std::map<std::string, std::filesystem::path> m_bases;
    std::reference_wrapper<log_sink> m_log;
};

}

#endif
