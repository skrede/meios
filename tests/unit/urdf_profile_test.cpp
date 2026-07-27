#include "profile_table.h"

#include <meios/urdf.h>
#include <meios/model.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/source_location.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <algorithm>
#include <filesystem>

namespace
{

struct captured
{
    meios::level             lvl;
    meios::diagnostic_code   code;
    meios::source_location   loc;
};

struct recorder
{
    std::vector<captured> &sink;

    void operator()(meios::level lvl, const std::string &)
    {
        sink.push_back({ lvl, meios::diagnostic_code::unspecified, {} });
    }

    void operator()(meios::level lvl, const meios::source_location &loc, const std::string &)
    {
        sink.push_back({ lvl, meios::diagnostic_code::unspecified, loc });
    }

    void operator()(meios::level lvl, meios::diagnostic_code code, const meios::source_location &loc,
                    const std::string &)
    {
        sink.push_back({ lvl, code, loc });
    }
};

std::vector<captured> load(const std::string &fixture)
{
    std::vector<captured> out;
    meios::log_sink_f capture{ recorder{ out } };
    const std::filesystem::path path =
        std::filesystem::path{ MEIOS_URDF_FIXTURE_DIR } / "profile" / fixture;
    meios::load(path, meios::load_options{}, capture);
    return out;
}

bool located_refusal(const std::vector<captured> &diagnostics, const std::string &code)
{
    return std::any_of(diagnostics.begin(), diagnostics.end(), [&code](const captured &d) {
        return d.lvl == meios::level::error && meios::to_string(d.code) == code
            && !d.loc.file.empty() && d.loc.line > 0;
    });
}

std::size_t errors(const std::vector<captured> &diagnostics)
{
    return static_cast<std::size_t>(
        std::count_if(diagnostics.begin(), diagnostics.end(),
                      [](const captured &d) { return d.lvl == meios::level::error; }));
}

}

TEST_CASE("the rule table reaches the runner whole", "[urdf][profile]")
{
    const std::vector<profile::row> rows = profile::load_rows();
    REQUIRE(rows.size() >= 3);
    for(const profile::row &r : rows)
    {
        INFO(r.fixture);
        REQUIRE(profile::declared_verdict(r.verdict));
        REQUIRE_FALSE(r.rule.empty());
    }
}

TEST_CASE("every refusing row is refused under its own code at a real location", "[urdf][profile]")
{
    for(const profile::row &r : profile::load_rows())
    {
        if(r.verdict != "refuse")
            continue;
        INFO(r.fixture);
        REQUIRE(located_refusal(load(r.fixture), r.code));
    }
}

TEST_CASE("every accepting row loads without an error diagnostic", "[urdf][profile]")
{
    for(const profile::row &r : profile::load_rows())
    {
        if(r.verdict != "accept")
            continue;
        INFO(r.fixture);
        REQUIRE(errors(load(r.fixture)) == 0);
    }
}
