#include "uri_table.h"
#include "uri_fixture.h"
#include "uri_sandbox.h"

#include <meios/urdf.h>
#include <meios/expected.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/load_error.h>
#include <meios/diagnostic/missing_asset.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <utility>
#include <optional>
#include <algorithm>
#include <filesystem>
#include <string_view>

namespace
{

std::optional<uri::row> row_for_rule(std::string_view slug)
{
    for(const uri::row &r : uri::load_rows())
    {
        if(r.rule == slug)
            return r;
    }
    return std::nullopt;
}

std::vector<uri::row> rows_with_code(std::string_view code)
{
    std::vector<uri::row> out;
    for(const uri::row &r : uri::load_rows())
    {
        if(r.code == code)
            out.push_back(r);
    }
    return out;
}

struct driven_load
{
    bool succeeded;
    std::vector<uri::captured> raised;
};

// A source layer has no document position, so a refusal it raises carries an empty location, which
// uri::located would miss for that reason rather than for the reason under test. An absent level
// counts a code at every level, which is what an absence claim needs to say.
std::size_t recorded(const std::vector<uri::captured> &diagnostics, const std::string &code, std::optional<meios::level> lvl = std::nullopt)
{
    return static_cast<std::size_t>(
            std::count_if(diagnostics.begin(), diagnostics.end(), [&code, lvl](const uri::captured &d) { return meios::to_string(d.code) == code && (!lvl || d.lvl == *lvl); }));
}

driven_load drive_empty_segment(std::string_view reference, meios::missing_asset setting)
{
    const uri::sandbox tree;
    const std::filesystem::path document = tree.write_document(R"(<?xml version="1.0"?>)"
                                                               R"(<robot name="empty_segment"><link name="base_link"><visual><geometry>)"
                                                               R"(<mesh filename=")" +
                                                               std::string(reference) + R"("/></geometry></visual></link></robot>)");
    meios::load_options opts;
    opts.on_missing = setting;
    opts.package_roots.push_back(tree.root());
    std::vector<uri::captured> raised;
    meios::log_sink_f capture{uri::recorder{raised}};
    const meios::expected<meios::load_result, meios::load_error> loaded = meios::load(document, opts, capture);
    return {static_cast<bool>(loaded), std::move(raised)};
}

}

// One of the three settings is the default, so an assertion driving only that one would pass
// while a containment refusal graded by policy violated the contract it is meant to pin.
TEST_CASE("uri rows unsoftened containment refusal", "[urdf][uri]")
{
    constexpr meios::missing_asset settings[] = {meios::missing_asset::skip, meios::missing_asset::warn, meios::missing_asset::fail};
    const std::vector<uri::row> uncontained   = rows_with_code("uncontained_asset");
    REQUIRE_FALSE(uncontained.empty());
    for(const uri::row &r : uncontained)
    {
        for(meios::missing_asset setting : settings)
        {
            INFO(r.fixture);
            CHECK(uri::located(uri::load_fixture(r.fixture, setting).diagnostics, meios::level::error, r.code));
        }
    }
}

// Containment that refuses everything is not a containment rule, and it would satisfy every
// refusal assertion in this file; this is the row that fails under one.
TEST_CASE("uri rows absolute inside a registered root", "[urdf][uri]")
{
    const std::optional<uri::row> inside = row_for_rule("absolute-path-inside-a-root");
    REQUIRE(inside);
    INFO(inside->fixture);
    const uri::outcome result = uri::load_fixture(inside->fixture);
    CHECK(result.succeeded);
    CHECK(uri::errors(result.diagnostics) == 0);
    CHECK(uri::resolved_mesh_path(result.robot).has_value());
}

// The two absent codes are the ones a lookup and a containment decision would have produced, and
// the missing-asset policy grades one of them, so their absence at every setting is what shows the
// shape was decided before any of the three was consulted.
TEST_CASE("uri package empty segment refused by shape", "[urdf][uri]")
{
    constexpr meios::missing_asset settings[] = {meios::missing_asset::skip, meios::missing_asset::warn, meios::missing_asset::fail};
    for(std::string_view reference : {"package://somepkg//", "package://somepkg//meshes/base.stl"})
    {
        for(meios::missing_asset setting : settings)
        {
            INFO(reference << " at missing-asset setting " << static_cast<int>(setting));
            const driven_load run = drive_empty_segment(reference, setting);
            CHECK_FALSE(run.succeeded);
            CHECK(recorded(run.raised, "malformed_asset_uri", meios::level::error) > 0);
            CHECK(recorded(run.raised, "uncontained_asset") == 0);
            CHECK(recorded(run.raised, "unresolved_asset") == 0);
        }
    }
}
