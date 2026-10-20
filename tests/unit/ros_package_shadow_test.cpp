#include "xacro_include_tree.h"

#include <meios/ros/ros_package_source.h>

#include <meios/io/source_stack.h>
#include <meios/io/directory_source.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/source_location.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <utility>
#include <algorithm>
#include <filesystem>
#include <string_view>

namespace
{

struct level_recorder final : public meios::log_sink
{
    std::vector<std::pair<meios::level, std::string>> heard;

    using meios::log_sink::log;

    void log(meios::level lvl, const std::string &message) override
    {
        heard.emplace_back(lvl, message);
    }

    void log(meios::level lvl, const meios::source_location &, const std::string &message) override
    {
        heard.emplace_back(lvl, message);
    }

    std::size_t count(meios::level lvl, std::string_view fragment) const
    {
        return static_cast<std::size_t>(std::count_if(heard.begin(), heard.end(), [&](const auto &one) { return one.first == lvl && one.second.find(fragment) != std::string::npos; }));
    }
};

}

TEST_CASE("a lookup a directory layer answered is not refused by a ROS crawl of the same root that would reject it", "[ros][link]")
{
    const include_tree::tree held;
    const std::filesystem::path root   = held.root() / "ws";
    const std::filesystem::path target = held.write("ws/q/a.xacro", include_tree::binds("in_root"));
    held.write("ws/p/package.xml", "<package><name>p</name></package>");
    held.write("ws/p/plain.xacro", include_tree::binds("plain"));
    std::filesystem::create_directories(root / "p" / "urdf");
    if(!include_tree::linked(target, root / "p" / "urdf" / "a.xacro"))
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }
    for(const std::string_view filename : {"$(find p)/urdf/a.xacro", "package://p/urdf/a.xacro"})
        DYNAMIC_SECTION(filename)
        {
            level_recorder log;
            meios::source_stack sources{meios::directory_source{root, log}, meios::ros_package_source({root}, {}, log)};
            const std::string text = include_tree::robot(include_tree::include("package://p/plain.xacro") + include_tree::include(filename) + "<link name=\"${leaf}\"/>");
            const include_tree::expansion_result out = include_tree::expand_text(text, held.root() / "doc" / "robot.xacro", sources, log);
            for(const auto &[lvl, message] : log.heard)
                UNSCOPED_INFO(meios::to_string(lvl) << ": " << message);
            CHECK(include_tree::renders_link(out, "in_root"));
            CHECK(log.count(meios::level::error, "") == 0);
            CHECK(log.count(meios::level::info, "\"p/plain.xacro\" also resolved") == 1);
            CHECK(log.count(meios::level::info, "\"p/urdf/a.xacro\" also resolved") == 0);
        }
}
