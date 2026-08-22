#include <meios/urdf.h>
#include <meios/model.h>
#include <meios/io.h>
#include <meios/xacro.h>

#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>

namespace
{

std::string slurp(const std::string &name)
{
    const std::filesystem::path path = std::filesystem::path{ MEIOS_URDF_FIXTURE_DIR } / name;
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

struct capture_result
{
    std::vector<meios::level> levels;
    std::vector<meios::diagnostic_code> codes;
    std::size_t links;
    meios::vector3<double> offset;
};

struct recorder
{
    capture_result &sink;

    void operator()(meios::level lvl, const std::string &)
    {
        sink.levels.push_back(lvl);
        sink.codes.push_back(meios::diagnostic_code::unspecified);
    }

    void operator()(meios::level lvl, const meios::source_location &, const std::string &)
    {
        sink.levels.push_back(lvl);
        sink.codes.push_back(meios::diagnostic_code::unspecified);
    }

    void operator()(meios::level lvl, meios::diagnostic_code code, const meios::source_location &,
                    const std::string &)
    {
        sink.levels.push_back(lvl);
        sink.codes.push_back(code);
    }
};

capture_result run(const std::string &fixture, meios::strictness strict)
{
    capture_result out{};
    meios::log_sink_f capture{ recorder{ out } };
    meios::source_stack sources{};
    meios::core_evaluator eval;
    meios::parse_context ctx{ sources, eval, capture, meios::missing_asset::warn,
                              meios::topology_policy::fail, meios::material_policy::warn, strict, {} };
    meios::pod_recorder<meios::tree<double>> rec(capture, meios::topology_policy::fail);
    meios::basic_parser<meios::urdf_reader> parser(ctx);
    parser.parse(slurp(fixture), rec);
    out.links = rec.result().links.size();
    if(!rec.result().joints.empty())
        out.offset = rec.result().joints.front().origin.translation;
    return out;
}

bool has(const capture_result &result, meios::level lvl, meios::diagnostic_code code)
{
    for(std::size_t i = 0; i < result.levels.size(); ++i)
        if(result.levels[i] == lvl && result.codes[i] == code)
            return true;
    return false;
}

bool has_code(const capture_result &result, meios::diagnostic_code code)
{
    return std::find(result.codes.begin(), result.codes.end(), code) != result.codes.end();
}

int errors(const capture_result &result)
{
    return static_cast<int>(std::count(result.levels.begin(), result.levels.end(), meios::level::error));
}

struct hygiene_case
{
    std::string fixture;
    meios::diagnostic_code code;
};

std::vector<hygiene_case> hygiene_cases()
{
    return { { "dup_attr.urdf", meios::diagnostic_code::duplicate_attribute },
             { "multi_root_xml.urdf", meios::diagnostic_code::additional_root_element },
             { "trailing_garbage.urdf", meios::diagnostic_code::trailing_content },
             { "comment_in_value.urdf", meios::diagnostic_code::comment_interrupting } };
}

const std::string short_offset = "profile/origin_xyz_short.urdf";

}

TEST_CASE("each document-validity setting grades an xml-hygiene class as its name says", "[urdf][strict]")
{
    SECTION("fail reports the class as an error and stops before a link is emitted")
    {
        for(const hygiene_case &c : hygiene_cases())
        {
            INFO(c.fixture);
            const capture_result out = run(c.fixture, meios::strictness::fail);
            REQUIRE(has(out, meios::level::error, c.code));
            REQUIRE(out.links == 0);
        }
    }

    SECTION("warn reports the same class as a warning and keeps the document")
    {
        for(const hygiene_case &c : hygiene_cases())
        {
            INFO(c.fixture);
            const capture_result out = run(c.fixture, meios::strictness::warn);
            REQUIRE(has(out, meios::level::warn, c.code));
            REQUIRE(errors(out) == 0);
            REQUIRE(out.links >= 1);
        }
    }

    SECTION("skip reports the class at no level at all and keeps the document")
    {
        for(const hygiene_case &c : hygiene_cases())
        {
            INFO(c.fixture);
            const capture_result out = run(c.fixture, meios::strictness::skip);
            REQUIRE_FALSE(has_code(out, c.code));
            REQUIRE(out.links >= 1);
        }
    }
}

TEST_CASE("the arity refusal answers to the same setting and supplies nothing at any of them",
          "[urdf][strict]")
{
    SECTION("fail carries the arity code at error level")
    {
        const capture_result out = run(short_offset, meios::strictness::fail);
        REQUIRE(has(out, meios::level::error, meios::diagnostic_code::vector_arity));
        REQUIRE(out.offset.y == 0.0);
    }

    SECTION("warn carries the same code at warning level and nothing at error level")
    {
        const capture_result out = run(short_offset, meios::strictness::warn);
        REQUIRE(has(out, meios::level::warn, meios::diagnostic_code::vector_arity));
        REQUIRE(errors(out) == 0);
        REQUIRE(out.links == 2);
        REQUIRE(out.offset.y == 0.0);
    }

    SECTION("skip carries the code at no level yet still declines to invent the third component")
    {
        const capture_result out = run(short_offset, meios::strictness::skip);
        REQUIRE_FALSE(has_code(out, meios::diagnostic_code::vector_arity));
        REQUIRE(out.links == 2);
        REQUIRE(out.offset.y == 0.0);
    }
}

TEST_CASE("a fixture with no violation stays silent at every setting", "[urdf][strict]")
{
    for(const meios::strictness setting :
        { meios::strictness::fail, meios::strictness::warn, meios::strictness::skip })
    {
        const capture_result clean = run("named_material.urdf", setting);
        REQUIRE(clean.levels.empty());
        REQUIRE(clean.links == 1);
    }
}
