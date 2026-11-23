#include "cli_verbs_fixture.h"

#include "verbs/verbs.h"

#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <filesystem>

using namespace meios;
using meios::cli::verb_context;

namespace
{

std::string read_golden(const std::string &relative)
{
    std::ifstream in(std::filesystem::path(MEIOS_GOLDEN_DIR) / relative, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

}

TEST_CASE("cli_verbs: info summarizes links, joints, and both degree-of-freedom counts")
{
    verb_context ctx;
    ctx.id          = "info";
    ctx.positionals = {fixture("branched_all_joints.urdf")};
    cout_capture out;
    REQUIRE(cli::run_info(ctx) == 0);
    REQUIRE(out.str().find("total_dof:") != std::string::npos);
    REQUIRE(out.str().find("independent_dof:") != std::string::npos);
}

TEST_CASE("cli_verbs: info --format json emits a machine-readable summary")
{
    verb_context ctx;
    ctx.id                      = "info";
    ctx.positionals             = {fixture("branched_all_joints.urdf")};
    ctx.value_flags["--format"] = "json";
    cout_capture out;
    REQUIRE(cli::run_info(ctx) == 0);
    const std::string printed = out.str();
    REQUIRE(printed.find("\"name\":\"branched_all_joints\"") != std::string::npos);
    REQUIRE(printed.find("\"joints_by_kind\":") != std::string::npos);
    REQUIRE(printed.find("dof:") == std::string::npos);
    REQUIRE(printed.find("\"total_dof\":") != std::string::npos);
}

TEST_CASE("cli_verbs: info exits nonzero and prints each failure's typed code exactly once")
{
    struct row
    {
        std::string file;
        diagnostic_code code;
    };
    const std::vector<row> rows = {
            {fixture("malformed_xml.urdf"), diagnostic_code::xml_parse_error},
            {fixture("cycle.urdf"), diagnostic_code::no_root_cycle},
            {fixture("this_file_does_not_exist.urdf"), diagnostic_code::cannot_open},
            {fixture("undefined_property.xacro"), diagnostic_code::undefined_property},
    };

    for(const row &r : rows)
    {
        verb_context ctx;
        ctx.id          = "info";
        ctx.positionals = {r.file};
        cerr_capture err;
        const int code          = cli::run_info(ctx);
        const std::string token = "(" + std::string(to_string(r.code)) + ")";
        REQUIRE(code != 0);
        REQUIRE(occurrences(err.str(), token) == 1);
    }
}

TEST_CASE("cli_verbs: info rejects an unknown --format value")
{
    verb_context ctx;
    ctx.id                      = "info";
    ctx.positionals             = {fixture("branched_all_joints.urdf")};
    ctx.value_flags["--format"] = "yaml";
    cout_capture out;
    REQUIRE(cli::run_info(ctx) != 0);
    REQUIRE(out.str().empty());
}

TEST_CASE("cli_verbs: args exits zero over a document")
{
    verb_context ctx;
    ctx.id          = "args";
    ctx.positionals = {fixture("branched_all_joints.urdf")};
    cout_capture out;
    REQUIRE(cli::run_args(ctx) == 0);
}

TEST_CASE("cli_verbs: completion bash equals the golden and calls __complete")
{
    verb_context ctx;
    ctx.id          = "completion";
    ctx.positionals = {"bash"};
    cout_capture out;
    REQUIRE(cli::run_completion(ctx) == 0);
    const std::string script = out.str();
    REQUIRE(script.find("meios __complete") != std::string::npos);
    REQUIRE(script == read_golden("completion/meios.bash"));
}

TEST_CASE("cli_verbs: completion rejects an unknown shell")
{
    verb_context ctx;
    ctx.id          = "completion";
    ctx.positionals = {"powershell"};
    cout_capture out;
    REQUIRE(cli::run_completion(ctx) == 1);
}
