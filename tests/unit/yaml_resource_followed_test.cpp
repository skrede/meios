#include "yaml_resource_fixture.h"

#include <meios/xacro/include_options.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>
#include <meios/diagnostic/operation_failure.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <fstream>
#include <utility>
#include <iterator>
#include <optional>
#include <algorithm>
#include <filesystem>
#include <string_view>
#include <system_error>

using yaml_resource_test::seed_tree;

namespace
{

struct record
{
    meios::level lvl;
    meios::diagnostic_code code;
    std::optional<meios::operation_failure> cause;
    std::string message;
};

struct recorder
{
    std::vector<record> &sink;

    void operator()(meios::level lvl, const std::string &message)
    {
        sink.push_back({lvl, meios::diagnostic_code::unspecified, std::nullopt, message});
    }

    void operator()(meios::level lvl, const meios::source_location &, const std::string &message)
    {
        sink.push_back({lvl, meios::diagnostic_code::unspecified, std::nullopt, message});
    }

    void operator()(meios::level lvl, meios::diagnostic_code code, const meios::source_location &, const std::string &message)
    {
        sink.push_back({lvl, code, std::nullopt, message});
    }

    void operator()(meios::level lvl, meios::diagnostic_code code, const meios::source_location &, const meios::operation_failure &cause, const std::string &message)
    {
        sink.push_back({lvl, code, cause, message});
    }
};

struct fetched
{
    meios::text_resource_result text;
    std::vector<record> records;
};

fetched fetch(const std::filesystem::path &root, std::string_view spec, bool follow)
{
    std::vector<record> records;
    meios::log_sink_f sink{recorder{records}};
    meios::source_stack sources;
    const std::vector<std::filesystem::path> roots{root / "pkg"};
    meios::include_options includes;
    includes.follow_outside_links            = follow;
    const meios::text_resource_loader loader = meios::detail::make_yaml_text_loader(sources, roots, sink, meios::default_byte_ceiling, includes);
    meios::text_resource_result text         = loader(spec, root / "docs" / "robot.xacro");
    return fetched{std::move(text), std::move(records)};
}

std::vector<record> errors_of(const fetched &got)
{
    std::vector<record> errors;
    std::copy_if(got.records.begin(), got.records.end(), std::back_inserter(errors), [](const record &r) { return r.lvl == meios::level::error; });
    return errors;
}

bool ends_with_one(const fetched &got, meios::diagnostic_code code, std::string_view fragment)
{
    const std::vector<record> errors = errors_of(got);
    return !got.text.has_value() && got.text.error() == meios::text_resource_failure::unreachable && errors.size() == 1 && errors.front().code == code &&
            errors.front().message.find(fragment) != std::string::npos;
}

bool seeded_with_root_copy(const std::filesystem::path &root, const std::filesystem::path &relative, bool linked)
{
    std::filesystem::create_directories((root / "pkg" / relative).parent_path());
    std::ofstream(root / "pkg" / relative) << "from: root-copy";
    if(!linked)
        std::filesystem::remove_all(root);
    return linked;
}

bool directory_link(const std::filesystem::path &target, const std::filesystem::path &link)
{
    std::error_code ec;
    std::filesystem::create_directory_symlink(target, link, ec);
    return !ec;
}

bool file_link(const std::filesystem::path &target, const std::filesystem::path &link)
{
    std::error_code ec;
    std::filesystem::create_symlink(target, link, ec);
    return !ec;
}

}

TEST_CASE("a followed link to a directory lacking the spec ends the sweep where a configured root holds the name", "[yaml][resource]")
{
    const std::filesystem::path root = seed_tree();
    if(!seeded_with_root_copy(root, "linked/cfg.yaml", directory_link(root / "outside", root / "docs" / "linked")))
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }
    const fetched unset     = fetch(root, "linked/cfg.yaml", false);
    const fetched following = fetch(root, "linked/cfg.yaml", true);
    std::filesystem::remove_all(root);

    REQUIRE_FALSE(unset.text.has_value());
    CHECK(unset.text.error() == meios::text_resource_failure::uncontained);
    REQUIRE(errors_of(unset).size() == 1);
    REQUIRE(ends_with_one(following, meios::diagnostic_code::unresolved_asset, "could not resolve resource"));
}

TEST_CASE("a followed link naming a directory ends the sweep where a configured root holds the name", "[yaml][resource]")
{
    const std::filesystem::path root = seed_tree();
    std::filesystem::create_directories(root / "outside" / "adir");
    if(!seeded_with_root_copy(root, "cfg2.yaml", directory_link(root / "outside" / "adir", root / "docs" / "cfg2.yaml")))
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }
    const fetched unset     = fetch(root, "cfg2.yaml", false);
    const fetched following = fetch(root, "cfg2.yaml", true);
    std::filesystem::remove_all(root);

    REQUIRE_FALSE(unset.text.has_value());
    CHECK(unset.text.error() == meios::text_resource_failure::uncontained);
    REQUIRE(ends_with_one(following, meios::diagnostic_code::cannot_open, "cannot read resource"));
    REQUIRE(errors_of(following).front().cause.has_value());
}

TEST_CASE("a dangling link with outside links followed ends the sweep where a configured root holds the name", "[yaml][resource]")
{
    const std::filesystem::path root = seed_tree();
    if(!seeded_with_root_copy(root, "cfg3.yaml", file_link(root / "outside" / "cfg3.yaml", root / "docs" / "cfg3.yaml")))
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }
    const fetched following = fetch(root, "cfg3.yaml", true);
    std::filesystem::remove_all(root);

    REQUIRE(ends_with_one(following, meios::diagnostic_code::unresolved_asset, "could not resolve resource"));
}

TEST_CASE("a dangling link with outside links unset falls through to a configured root", "[yaml][resource]")
{
    const std::filesystem::path root = seed_tree();
    if(!seeded_with_root_copy(root, "cfg3.yaml", file_link(root / "outside" / "cfg3.yaml", root / "docs" / "cfg3.yaml")))
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }
    const fetched unset = fetch(root, "cfg3.yaml", false);
    std::filesystem::remove_all(root);

    REQUIRE(unset.text.has_value());
    CHECK(*unset.text == "from: root-copy");
    REQUIRE(errors_of(unset).empty());
}
