#include "meios/ros/ros_package_source.h"

#include "meios/io/directory_source.h"

#include "meios/diagnostic/level.h"

#include <pugixml.hpp>

#include <set>
#include <cctype>
#include <cstdlib>
#include <utility>
#include <system_error>

namespace meios
{

namespace
{

// The one sanctioned platform branch, isolated in the backend: ROS search lists
// join their entries with ';' on Windows and ':' everywhere else.
constexpr char path_list_separator()
{
#ifdef _WIN32
    return ';';
#else
    return ':';
#endif
}

std::vector<std::filesystem::path> split_search_path(std::string_view value)
{
    std::vector<std::filesystem::path> parts;
    const char separator = path_list_separator();
    std::size_t start = 0;
    while(start <= value.size())
    {
        std::size_t end = value.find(separator, start);
        if(end == std::string_view::npos)
            end = value.size();
        if(end > start)
            parts.emplace_back(value.substr(start, end - start));
        start = end + 1;
    }
    return parts;
}

std::string trimmed(std::string text)
{
    auto is_space = [](char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; };
    while(!text.empty() && is_space(text.front()))
        text.erase(text.begin());
    while(!text.empty() && is_space(text.back()))
        text.pop_back();
    return text;
}

std::filesystem::path package_manifest(const std::filesystem::path &dir)
{
    std::error_code ec;
    if(std::filesystem::is_regular_file(dir / "package.xml", ec))
        return dir / "package.xml";
    if(std::filesystem::is_regular_file(dir / "manifest.xml", ec))
        return dir / "manifest.xml";
    return {};
}

std::string package_name(const std::filesystem::path &manifest, log_sink &log)
{
    pugi::xml_document doc;
    if(!doc.load_file(manifest.c_str()))
    {
        log.log(level::warn, "could not parse a ROS package manifest for its <name>");
        return {};
    }
    return trimmed(doc.document_element().child("name").text().as_string());
}

bool is_ignored(const std::filesystem::path &dir)
{
    std::error_code ec;
    return std::filesystem::exists(dir / "CATKIN_IGNORE", ec)
        || std::filesystem::exists(dir / "AMENT_IGNORE", ec);
}

std::filesystem::recursive_directory_iterator crawl_iterator(
    const std::filesystem::path &root, std::error_code &ec)
{
    // follow_directory_symlink lets a colcon --symlink-install package dir (an
    // in-root symlink to a source tree) be discovered; the visited-set below
    // breaks the cycles that following links can otherwise loop on.
    const std::filesystem::directory_options options =
        std::filesystem::directory_options::skip_permission_denied
        | std::filesystem::directory_options::follow_directory_symlink;
    return std::filesystem::recursive_directory_iterator(root, options, ec);
}

void crawl_ros1_root(const std::filesystem::path &root,
                     std::map<std::string, std::filesystem::path> &out, log_sink &log)
{
    std::error_code ec;
    std::filesystem::recursive_directory_iterator it = crawl_iterator(root, ec);
    const std::filesystem::recursive_directory_iterator end;
    std::set<std::filesystem::path> seen;
    for(; !ec && it != end; it.increment(ec))
    {
        std::error_code probe;
        std::filesystem::path real = std::filesystem::weakly_canonical(it->path(), probe);
        if(probe || !it->is_directory(probe) || is_ignored(it->path()) || !seen.insert(real).second)
        {
            it.disable_recursion_pending();
            continue;
        }
        std::filesystem::path manifest = package_manifest(it->path());
        if(manifest.empty())
            continue;
        std::string name = package_name(manifest, log);
        if(!name.empty())
            out.emplace(std::move(name), std::move(real));
        it.disable_recursion_pending();
    }
}

// An empty prefix joined with "share" is a relative path, and a relative index path is listed
// against the process working directory rather than against a root the caller named.
void read_ament_prefix(const std::filesystem::path &prefix,
                       std::map<std::string, std::filesystem::path> &out)
{
    if(prefix.empty())
        return;
    const std::filesystem::path index =
        prefix / "share" / "ament_index" / "resource_index" / "packages";
    std::error_code ec;
    std::filesystem::directory_iterator entries(index, ec);
    if(ec)
        return;
    for(const std::filesystem::directory_entry &entry : entries)
        out.emplace(entry.path().filename().string(), prefix / "share" / entry.path().filename());
}

std::vector<std::filesystem::path> env_search_path(const char *name, log_sink &log)
{
    log.log(level::info, std::string("read of environment variable \"") + name + '"');
    const char *value = std::getenv(name);
    if(!value)
        return {};
    return split_search_path(value);
}

}

std::map<std::string, std::filesystem::path> detail::discover_packages(
    const std::vector<std::filesystem::path> &ros1_roots,
    const std::vector<std::filesystem::path> &ament_prefixes, log_sink &log)
{
    std::map<std::string, std::filesystem::path> bases;
    for(const std::filesystem::path &prefix : ament_prefixes)
        read_ament_prefix(absolute_base(prefix), bases);
    for(const std::filesystem::path &root : ros1_roots)
        crawl_ros1_root(absolute_base(root), bases, log);
    return bases;
}

ros_package_source ros_package_source::from_environment(log_sink &log)
{
    std::vector<std::filesystem::path> ros1_roots = env_search_path("ROS_PACKAGE_PATH", log);
    std::vector<std::filesystem::path> ament_prefixes = env_search_path("AMENT_PREFIX_PATH", log);
    return ros_package_source(std::move(ros1_roots), std::move(ament_prefixes), log);
}

}
