#include <meios/urdf.h>
#include <meios/model.h>
#include <meios/io.h>
#include <meios/xacro.h>

#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <filesystem>

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
    meios::diagnostic_code code;
    std::string file;
    int line;
    std::string msg;
};

struct capture
{
    std::vector<entry> &entries;

    void operator()(meios::level lvl, const std::string &msg)
    {
        entries.push_back({ lvl, meios::diagnostic_code::unspecified, "", 0, msg });
    }

    void operator()(meios::level lvl, const meios::source_location &loc, const std::string &msg)
    {
        entries.push_back({ lvl, meios::diagnostic_code::unspecified, loc.file.string(), loc.line, msg });
    }

    void operator()(meios::level lvl, meios::diagnostic_code code, const meios::source_location &loc,
                    const std::string &msg)
    {
        entries.push_back({ lvl, code, loc.file.string(), loc.line, msg });
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

bool reaches_sink_located(const std::vector<entry> &entries, meios::level lvl, meios::diagnostic_code expected)
{
    for(const entry &e : entries)
        if(e.lvl == lvl && !e.file.empty() && e.line > 0 && e.code == expected)
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
        meios::diagnostic_code code;
    };

    const std::vector<expectation> classes{
        { "multi_root.urdf", meios::level::error, meios::diagnostic_code::additional_root },
        { "cycle.urdf", meios::level::error, meios::diagnostic_code::link_on_cycle },
        { "orphan_joint.urdf", meios::level::error, meios::diagnostic_code::undeclared_link },
        { "unreachable_link.urdf", meios::level::error, meios::diagnostic_code::unreachable_link },
        { "multi_parent.urdf", meios::level::error, meios::diagnostic_code::multiple_parents },
        { "dup_attr.urdf", meios::level::error, meios::diagnostic_code::duplicate_attribute },
        { "multi_root_xml.urdf", meios::level::error, meios::diagnostic_code::additional_root_element },
        { "trailing_garbage.urdf", meios::level::error, meios::diagnostic_code::trailing_content },
        { "comment_in_value.urdf", meios::level::error, meios::diagnostic_code::comment_interrupting },
        { "unresolved_material.urdf", meios::level::warn, meios::diagnostic_code::undefined_material },
        { "package_mesh.urdf", meios::level::warn, meios::diagnostic_code::unresolved_mesh },
    };

    for(const expectation &klass : classes)
    {
        INFO("class fixture: " << klass.fixture);
        REQUIRE(reaches_sink_located(run(klass.fixture), klass.level, klass.code));
    }
}
