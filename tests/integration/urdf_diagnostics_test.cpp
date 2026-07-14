#include <meios/urdf.h>
#include <meios/model.h>
#include <meios/io.h>
#include <meios/xacro.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <string_view>

namespace
{

std::filesystem::path fixture_path(const std::string &name)
{
    return std::filesystem::path{ MEIOS_URDF_FIXTURE_DIR } / name;
}

std::string slurp(const std::string &name)
{
    std::ifstream in(fixture_path(name), std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

struct entry
{
    meios::level lvl;
    std::string file;
    int line;
    std::string msg;
};

struct capture
{
    std::vector<entry> &entries;

    void operator()(meios::level lvl, const std::string &msg)
    {
        entries.push_back({ lvl, "", 0, msg });
    }

    void operator()(meios::level lvl, const meios::source_location &loc, const std::string &msg)
    {
        entries.push_back({ lvl, loc.file.string(), loc.line, msg });
    }
};

std::vector<entry> run(const std::string &fixture)
{
    std::vector<entry> entries;
    meios::log_sink_f cap{ capture{ entries } };
    meios::source_stack sources{};
    meios::core_evaluator eval;
    meios::parse_context ctx{ sources,       eval, cap, meios::missing_asset::warn,
                              meios::topology_policy::fail, meios::material_policy::warn,
                              meios::strictness::strict,    fixture_path(fixture) };
    meios::pod_recorder<meios::tree<double>> rec(cap, meios::topology_policy::fail);
    meios::basic_parser<meios::urdf_reader> parser(ctx);
    parser.parse(slurp(fixture), rec);
    return entries;
}

bool reaches_sink_located(const std::vector<entry> &entries, meios::level lvl, std::string_view needle)
{
    for(const entry &e : entries)
        if(e.lvl == lvl && !e.file.empty() && e.line > 0 && e.msg.find(needle) != std::string::npos)
            return true;
    return false;
}

}

TEST_CASE("every error class reaches the recording sink located with the right level", "[urdf][diagnostics]")
{
    struct expectation
    {
        std::string fixture;
        meios::level level;
        std::string needle;
    };

    const std::vector<expectation> classes{
        { "multi_root.urdf", meios::level::error, "is an additional root" },
        { "cycle.urdf", meios::level::error, "cycle" },
        { "orphan_joint.urdf", meios::level::error, "undeclared link" },
        { "unreachable_link.urdf", meios::level::error, "unreachable" },
        { "multi_parent.urdf", meios::level::error, "more than one parent" },
        { "dup_attr.urdf", meios::level::error, "duplicate attribute" },
        { "multi_root_xml.urdf", meios::level::error, "additional root element" },
        { "trailing_garbage.urdf", meios::level::error, "trailing content" },
        { "comment_in_value.urdf", meios::level::error, "comment interrupting" },
        { "unresolved_material.urdf", meios::level::warn, "undefined material" },
        { "package_mesh.urdf", meios::level::warn, "could not resolve mesh" },
    };

    for(const expectation &klass : classes)
    {
        INFO("class fixture: " << klass.fixture);
        REQUIRE(reaches_sink_located(run(klass.fixture), klass.level, klass.needle));
    }
}
