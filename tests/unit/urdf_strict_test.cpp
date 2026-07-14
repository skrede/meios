#include <meios/urdf.h>
#include <meios/model.h>
#include <meios/io.h>
#include <meios/xacro.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <string_view>
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
    std::vector<std::string> msgs;
    std::size_t links;
};

struct recorder
{
    capture_result &sink;

    void operator()(meios::level lvl, const std::string &msg)
    {
        sink.levels.push_back(lvl);
        sink.msgs.push_back(msg);
    }

    void operator()(meios::level lvl, const meios::source_location &, const std::string &msg)
    {
        sink.levels.push_back(lvl);
        sink.msgs.push_back(msg);
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
    return out;
}

bool has(const capture_result &result, meios::level lvl, std::string_view needle)
{
    for(std::size_t i = 0; i < result.levels.size(); ++i)
        if(result.levels[i] == lvl && result.msgs[i].find(needle) != std::string::npos)
            return true;
    return false;
}

int errors(const capture_result &result)
{
    return static_cast<int>(std::count(result.levels.begin(), result.levels.end(), meios::level::error));
}

}

TEST_CASE("each pugixml-leniency class is a located error under strict", "[urdf][strict]")
{
    REQUIRE(has(run("dup_attr.urdf", meios::strictness::strict), meios::level::error, "duplicate attribute"));
    REQUIRE(has(run("multi_root_xml.urdf", meios::strictness::strict), meios::level::error, "additional root"));
    REQUIRE(has(run("trailing_garbage.urdf", meios::strictness::strict), meios::level::error, "trailing content"));
    REQUIRE(has(run("comment_in_value.urdf", meios::strictness::strict), meios::level::error, "comment interrupting"));
}

TEST_CASE("strict aborts the walk while lenient downgrades to a warning and continues", "[urdf][strict]")
{
    const capture_result strict = run("dup_attr.urdf", meios::strictness::strict);
    REQUIRE(strict.links == 0);

    for(const std::string &fixture :
        { std::string("dup_attr.urdf"), std::string("multi_root_xml.urdf"),
          std::string("trailing_garbage.urdf"), std::string("comment_in_value.urdf") })
    {
        const capture_result lenient = run(fixture, meios::strictness::lenient);
        REQUIRE(errors(lenient) == 0);
        REQUIRE(has(lenient, meios::level::warn, ""));
        REQUIRE(lenient.links >= 1);
    }
}

TEST_CASE("a clean fixture produces no strictness diagnostic", "[urdf][strict]")
{
    const capture_result clean = run("named_material.urdf", meios::strictness::strict);
    REQUIRE(clean.levels.empty());
    REQUIRE(clean.links == 1);
}
