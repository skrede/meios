#include "verbs/verbs.h"
#include "verbs/tree.h"

#include "meios/model/model.h"

#include "meios/records/link.h"
#include "meios/records/joint.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>

using namespace meios;
using meios::cli::verb_context;

namespace
{

std::string fixture(const std::string &name)
{
    return (std::filesystem::path(MEIOS_URDF_FIXTURE_DIR) / name).string();
}

std::string read_golden(const std::string &relative)
{
    std::ifstream in(std::filesystem::path(MEIOS_GOLDEN_DIR) / relative, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

class cout_capture
{
public:
    cout_capture() : m_buffer(), m_saved(std::cout.rdbuf(m_buffer.rdbuf())) {}

    ~cout_capture() { std::cout.rdbuf(m_saved); }

    cout_capture(const cout_capture &) = delete;
    cout_capture &operator=(const cout_capture &) = delete;

    std::string str() const { return m_buffer.str(); }

private:
    std::ostringstream m_buffer;
    std::streambuf *m_saved;
};

std::string run_tree_on(const std::string &file, bool dot)
{
    verb_context ctx;
    ctx.id = "tree";
    ctx.positionals = { file };
    ctx.bool_flags["--dot"] = dot;
    cout_capture out;
    REQUIRE(cli::run_tree(ctx) == 0);
    return out.str();
}

}

TEST_CASE("cli_tree: the branched fixture renders every branch to the ASCII golden")
{
    const std::string rendered = run_tree_on(fixture("branched_all_joints.urdf"), false);
    REQUIRE(rendered == read_golden("cli_tree/branched.txt"));
    REQUIRE(rendered.find("├─ ") != std::string::npos);
    REQUIRE(rendered.find("└─ ") != std::string::npos);
}

TEST_CASE("cli_tree: both branches of a link render, not a single chain")
{
    const std::string rendered = run_tree_on(fixture("branched_all_joints.urdf"), false);
    REQUIRE(rendered.find("arm") != std::string::npos);
    REQUIRE(rendered.find("slider") != std::string::npos);
    REQUIRE(rendered.find("free_body") != std::string::npos);
    REQUIRE(rendered.find("plane_body") != std::string::npos);
}

TEST_CASE("cli_tree: --dot matches the golden with movable and fixed styled distinctly")
{
    const std::string rendered = run_tree_on(fixture("branched_all_joints.urdf"), true);
    REQUIRE(rendered == read_golden("cli_tree/branched.dot"));
    REQUIRE(rendered.find("revolute\", style=solid") != std::string::npos);
    REQUIRE(rendered.find("fixed\", style=dashed") != std::string::npos);
}

TEST_CASE("cli_tree: a loop-closure edge is styled distinctly")
{
    model<double> robot;
    robot.links.push_back(link<double>{ "a", {}, {}, {}, {}, {} });
    robot.links.push_back(link<double>{ "b", {}, {}, {}, {}, {} });
    joint<double> loop{};
    loop.name = "belt";
    loop.kind = joint_kind::fixed;
    loop.parent = "a";
    loop.child = "b";
    loop.closes_loop = true;
    robot.joints.push_back(loop);

    const std::string dot = cli::render_dot(robot);
    REQUIRE(dot.find("style=dotted") != std::string::npos);
    REQUIRE(dot.find("color=red") != std::string::npos);
}

TEST_CASE("cli_tree: a hostile link name is escaped in DOT output")
{
    const std::string dot = run_tree_on(fixture("hostile_name.urdf"), true);
    REQUIRE(dot.find("evil\\\"") != std::string::npos);
    REQUIRE(dot.find("system(\\n)") != std::string::npos);
    REQUIRE(dot.find("evil\"") == std::string::npos);
    REQUIRE(dot.find("system(\n)") == std::string::npos);
}

TEST_CASE("cli_tree: a hostile link name is sanitized in ASCII output")
{
    const std::string ascii = run_tree_on(fixture("hostile_name.urdf"), false);
    REQUIRE(ascii.find("system(\n)") == std::string::npos);
    REQUIRE(ascii.find("system(?)") != std::string::npos);
}
