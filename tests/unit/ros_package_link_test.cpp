#include "xacro_include_tree.h"
#include "ros_package_fixture.h"

#include <meios/ros/ros_package_source.h>

#include <meios/xacro.h>

#include <meios/io/source_stack.h>
#include <meios/io/directory_source.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>
#include <meios/diagnostic/source_location.h>
#include <meios/diagnostic/operation_failure.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include <filesystem>
#include <string_view>
#include <system_error>

namespace
{

using include_tree::expansion_result;
using ros_test::portable_setenv;

struct message_record
{
    meios::level lvl;
    meios::diagnostic_code code;
    std::string message;
};

struct message_recorder
{
    void operator()(meios::level lvl, const std::string &message)
    {
        sink.push_back({lvl, meios::diagnostic_code::unspecified, message});
    }

    void operator()(meios::level lvl, const meios::source_location &, const std::string &message)
    {
        sink.push_back({lvl, meios::diagnostic_code::unspecified, message});
    }

    void operator()(meios::level lvl, meios::diagnostic_code code, const meios::source_location &, const std::string &message)
    {
        sink.push_back({lvl, code, message});
    }

    void operator()(meios::level lvl, meios::diagnostic_code code, const meios::source_location &, const meios::operation_failure &, const std::string &message)
    {
        sink.push_back({lvl, code, message});
    }

    std::vector<message_record> &sink;
};

std::size_t errors(const std::vector<message_record> &records)
{
    return static_cast<std::size_t>(std::count_if(records.begin(), records.end(), [](const message_record &r) { return r.lvl == meios::level::error; }));
}

meios::ros_package_source from_package_path(const std::filesystem::path &entry, meios::log_sink &log)
{
    portable_setenv("ROS_PACKAGE_PATH", entry.string().c_str());
    portable_setenv("AMENT_PREFIX_PATH", "");
    meios::ros_package_source source = meios::ros_package_source::from_environment(log);
    portable_setenv("ROS_PACKAGE_PATH", "");
    return source;
}

std::filesystem::path package_p(const include_tree::tree &held)
{
    held.write("ws/p/package.xml", "<package><name>p</name></package>");
    return held.root() / "ws";
}

template<typename Source>
expansion_result expand_through(Source source, const include_tree::tree &held, std::string_view filename, meios::log_sink &log)
{
    meios::source_stack sources{std::move(source)};
    return include_tree::expand_text(include_tree::reads_leaf(filename), held.root() / "doc" / "robot.xacro", sources, log);
}

bool renders(const expansion_result &out, std::string_view name)
{
    if(!out)
        UNSCOPED_INFO(meios::to_string(out.error().code) << ": " << out.error().message);
    return include_tree::renders_link(out, name);
}

}

TEST_CASE("an in-package link reads through ROS_PACKAGE_PATH as through a configured root", "[ros][link]")
{
    const include_tree::tree held;
    const std::filesystem::path ws     = package_p(held);
    const std::filesystem::path target = held.write("ws/p/real/a.xacro", include_tree::binds("inroot"));
    if(!include_tree::linked(target, ws / "p" / "a.xacro"))
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }
    std::vector<message_record> records;
    meios::log_sink_f log{message_recorder{records}};

    CHECK(renders(expand_through(from_package_path(ws, log), held, "package://p/a.xacro", log), "inroot"));
    CHECK(renders(expand_through(from_package_path(ws, log), held, "$(find p)/a.xacro", log), "inroot"));
    CHECK(renders(expand_through(meios::directory_source{ws, log}, held, "package://p/a.xacro", log), "inroot"));
    REQUIRE(errors(records) == 0);
}

TEST_CASE("a package under a ROS_PACKAGE_PATH entry that is itself a link reads", "[ros][link]")
{
    const include_tree::tree held;
    const std::filesystem::path ws = package_p(held);
    held.write("ws/p/plain.xacro", include_tree::binds("plain"));
    std::error_code ec;
    std::filesystem::create_directory_symlink(ws, held.root() / "wslink", ec);
    if(ec)
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }
    std::vector<message_record> records;
    meios::log_sink_f log{message_recorder{records}};

    CHECK(renders(expand_through(from_package_path(held.root() / "wslink", log), held, "package://p/plain.xacro", log), "plain"));
    REQUIRE(errors(records) == 0);
}

TEST_CASE("a document the ROS layer served through an in-package link anchors its includes where it was named", "[ros][link]")
{
    const include_tree::tree held;
    const std::filesystem::path ws     = package_p(held);
    const std::filesystem::path target = held.write("ws/p/q/a.xacro", include_tree::robot(include_tree::include("b.xacro")));
    held.write("ws/p/q/b.xacro", include_tree::binds("canonical_dir_b"));
    held.write("ws/p/urdf/b.xacro", include_tree::binds("named_dir_b"));
    if(!include_tree::linked(target, ws / "p" / "urdf" / "a.xacro"))
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }
    std::vector<message_record> records;
    meios::log_sink_f log{message_recorder{records}};

    CHECK(renders(expand_through(from_package_path(ws, log), held, "$(find p)/urdf/a.xacro", log), "named_dir_b"));
    REQUIRE(errors(records) == 0);
}

TEST_CASE("a link leading out of its ROS package still refuses as uncontained", "[ros][link]")
{
    const include_tree::tree held;
    const std::filesystem::path ws      = package_p(held);
    const std::filesystem::path outside = held.write("outside/out.xacro", include_tree::binds("outside"));
    if(!include_tree::linked(outside, ws / "p" / "out.xacro"))
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }
    std::vector<message_record> records;
    meios::log_sink_f log{message_recorder{records}};

    const expansion_result out = expand_through(from_package_path(ws, log), held, "package://p/out.xacro", log);
    REQUIRE_FALSE(out.has_value());
    REQUIRE(out.error().code == meios::diagnostic_code::unresolved_include);
    const auto uncontained = [](const message_record &r) { return r.lvl == meios::level::error && r.code == meios::diagnostic_code::uncontained_asset; };
    REQUIRE(std::count_if(records.begin(), records.end(), uncontained) == 1);
    const auto record = std::find_if(records.begin(), records.end(), uncontained);
    INFO(record->message);
    REQUIRE(record->message.find("\"p/out.xacro\"") != std::string::npos);
}
