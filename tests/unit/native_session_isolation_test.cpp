#include <meios/urdf/load.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <thread>
#include <cstdint>
#include <fstream>
#include <optional>
#include <filesystem>
#include <string_view>

namespace
{

struct outcome
{
    std::optional<std::string> link;
    std::optional<meios::load_error> error;
};

std::filesystem::path scratch_root()
{
    return std::filesystem::temp_directory_path() / "meios-native-session-isolation";
}

std::filesystem::path write_document(std::string_view name, std::string_view link)
{
    std::filesystem::create_directories(scratch_root());
    const std::filesystem::path path = scratch_root() / std::string(name);
    std::ofstream out(path, std::ios::binary);
    out << "<robot name=\"r\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">\n"
        << "<xacro:property name=\"present\" value=\"2\"/>\n"
        << "<link name=\"" << link << "\"/>\n</robot>\n";
    return path;
}

outcome load_one(const std::filesystem::path &path, const meios::load_options &shared)
{
    const meios::expected<meios::load_result, meios::load_error> loaded = meios::load(path, shared);
    if(!loaded)
        return outcome{std::nullopt, loaded.error()};
    if(loaded->robot.links.size() != 1)
        return outcome{};
    return outcome{loaded->robot.links.front().name, std::nullopt};
}

void require_failed_alone(const outcome &failed, const std::filesystem::path &path)
{
    REQUIRE(failed.error.has_value());
    CHECK_FALSE(failed.link.has_value());
    CHECK(failed.error->loc.file.filename() == path.filename());
    CHECK(failed.error->loc.line == 3);
    for(const meios::captured_diagnostic &one : failed.error->diagnostics)
        CHECK(one.loc.file.filename() != "good.xacro");
}

void require_succeeded_alone(const outcome &succeeded)
{
    CHECK_FALSE(succeeded.error.has_value());
    CHECK(succeeded.link == std::optional<std::string>("link2"));
}

}

TEST_CASE("a failing load and a succeeding load over one configuration each report their own outcome", "[native][session]")
{
    const std::filesystem::path bad  = write_document("bad.xacro", "link${missing}");
    const std::filesystem::path good = write_document("good.xacro", "link${present}");
    const meios::load_options shared;

    SECTION("one after the other, the failure first")
    {
        require_failed_alone(load_one(bad, shared), bad);
        require_succeeded_alone(load_one(good, shared));
    }
    SECTION("one after the other, the success first")
    {
        require_succeeded_alone(load_one(good, shared));
        require_failed_alone(load_one(bad, shared), bad);
    }
    SECTION("at the same time")
    {
        for(std::int32_t round = 0; round < 16; ++round)
        {
            outcome failed;
            outcome succeeded;
            std::thread one([&] { failed = load_one(bad, shared); });
            std::thread two([&] { succeeded = load_one(good, shared); });
            one.join();
            two.join();
            require_failed_alone(failed, bad);
            require_succeeded_alone(succeeded);
        }
    }

    std::filesystem::remove_all(scratch_root());
}
