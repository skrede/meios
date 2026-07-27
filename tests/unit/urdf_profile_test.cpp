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

struct outcome
{
    std::vector<captured> diagnostics;
    bool                  succeeded;
    meios::completeness   claims;
};

outcome load(const std::string &fixture)
{
    std::vector<captured> out;
    meios::log_sink_f capture{ recorder{ out } };
    const std::filesystem::path path =
        std::filesystem::path{ MEIOS_URDF_FIXTURE_DIR } / "profile" / fixture;
    const meios::expected<meios::load_result, meios::load_error> loaded =
        meios::load(path, meios::load_options{}, capture);
    if(!loaded)
        return { std::move(out), false, meios::completeness::none };
    return { std::move(out), true, loaded->claims };
}

bool located(const std::vector<captured> &diagnostics, meios::level lvl, const std::string &code)
{
    return std::any_of(diagnostics.begin(), diagnostics.end(), [lvl, &code](const captured &d) {
        return d.lvl == lvl && meios::to_string(d.code) == code && !d.loc.file.empty()
            && d.loc.line > 0;
    });
}

meios::diagnostic_code code_from(const std::string &name)
{
    for(int value = 0; meios::to_string(static_cast<meios::diagnostic_code>(value)) != "unknown";
        ++value)
    {
        const meios::diagnostic_code code = static_cast<meios::diagnostic_code>(value);
        if(meios::to_string(code) == name)
            return code;
    }
    return meios::diagnostic_code::unspecified;
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
        REQUIRE(located(load(r.fixture).diagnostics, meios::level::error, r.code));
    }
}

TEST_CASE("every dropping row discloses its code, still loads, and answers for the parse claim",
          "[urdf][profile]")
{
    for(const profile::row &r : profile::load_rows())
    {
        if(r.verdict != "drop")
            continue;
        INFO(r.fixture);
        const outcome result = load(r.fixture);
        REQUIRE(result.succeeded);
        REQUIRE(errors(result.diagnostics) == 0);
        REQUIRE(located(result.diagnostics, meios::level::warn, r.code));
        const meios::completeness cleared = meios::cleared_by(code_from(r.code));
        REQUIRE(meios::has(result.claims, meios::completeness::parsed)
                == !meios::has(cleared, meios::completeness::parsed));
    }
}

TEST_CASE("every accepting row loads without an error diagnostic", "[urdf][profile]")
{
    for(const profile::row &r : profile::load_rows())
    {
        if(r.verdict != "accept")
            continue;
        INFO(r.fixture);
        const outcome result = load(r.fixture);
        REQUIRE(result.succeeded);
        REQUIRE(errors(result.diagnostics) == 0);
    }
}
