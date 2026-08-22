#include "verbs/verbs.h"
#include "verbs/tree.h"

#include "meios/model/model.h"

#include "meios/records/link.h"
#include "meios/records/joint.h"

#include "meios/diagnostic/diagnostic_code.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
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

// The stderr twin of cout_capture: run_tree relays typed diagnostics through a
// log_sink_s bound to std::cerr, so a failing-load test swaps std::cerr's rdbuf.
class cerr_capture
{
public:
    cerr_capture() : m_buffer(), m_saved(std::cerr.rdbuf(m_buffer.rdbuf())) {}

    ~cerr_capture() { std::cerr.rdbuf(m_saved); }

    cerr_capture(const cerr_capture &) = delete;
    cerr_capture &operator=(const cerr_capture &) = delete;

    std::string str() const { return m_buffer.str(); }

private:
    std::ostringstream m_buffer;
    std::streambuf *m_saved;
};

std::size_t occurrences(const std::string &haystack, const std::string &needle)
{
    std::size_t count = 0;
    for(std::size_t pos = haystack.find(needle); pos != std::string::npos;
        pos = haystack.find(needle, pos + needle.size()))
        ++count;
    return count;
}

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

struct tree_run
{
    int code;
    std::string out;
};

tree_run run_tree_root(const std::string &file, const std::string &root)
{
    verb_context ctx;
    ctx.id = "tree";
    ctx.positionals = { file };
    ctx.value_flags["--root"] = root;
    cout_capture out;
    const int code = cli::run_tree(ctx);
    return { code, out.str() };
}

std::size_t line_count(const std::string &text)
{
    return static_cast<std::size_t>(std::count(text.begin(), text.end(), '\n'));
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

TEST_CASE("cli_tree: --root renders only the subtree rooted at the named link")
{
    const std::string full = run_tree_on(fixture("branched_all_joints.urdf"), false);
    const tree_run sub = run_tree_root(fixture("branched_all_joints.urdf"), "torso");
    REQUIRE(sub.code == 0);
    REQUIRE(sub.out.find("arm") != std::string::npos);
    REQUIRE(sub.out.find("slider") != std::string::npos);
    REQUIRE(sub.out.find("free_body") == std::string::npos);
    REQUIRE(sub.out.find("plane_body") == std::string::npos);
    REQUIRE(line_count(sub.out) < line_count(full));
}

TEST_CASE("cli_tree: an unknown --root link exits nonzero and renders nothing")
{
    const tree_run sub = run_tree_root(fixture("branched_all_joints.urdf"), "no_such_link");
    REQUIRE(sub.code != 0);
    REQUIRE(sub.out.empty());
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
    // Qualify to disambiguate from POSIX ::link (unistd.h) under `using namespace
    // meios;` on gcc/Linux, where an unqualified `link` is ambiguous.
    robot.links.push_back(meios::link<double>{ "a", {}, {}, {}, {}, {} });
    robot.links.push_back(meios::link<double>{ "b", {}, {}, {}, {}, {} });
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

TEST_CASE("cli_tree: tree exits nonzero and prints each failure's typed code exactly once")
{
    // No topology-error row: run_tree loads under topology_policy::warn, so a cyclic
    // URDF loads and exits zero by design. Asserting exit != 0 there would fail
    // against correct code.
    struct row
    {
        std::string file;
        diagnostic_code code;
    };
    const std::vector<row> rows = {
        { fixture("malformed_xml.urdf"), diagnostic_code::xml_parse_error },
        { fixture("this_file_does_not_exist.urdf"), diagnostic_code::cannot_open },
        { fixture("undefined_property.xacro"), diagnostic_code::undefined_property },
    };

    for(const row &r : rows)
    {
        verb_context ctx;
        ctx.id = "tree";
        ctx.positionals = { r.file };
        cerr_capture err;
        const int code = cli::run_tree(ctx);
        const std::string token = "(" + std::string(to_string(r.code)) + ")";
        REQUIRE(code != 0);
        REQUIRE(occurrences(err.str(), token) == 1);
    }
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
