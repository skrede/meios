#include "verbs/verbs.h"

#include "xacro_include_tree.h"
#include "ros_package_fixture.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <string_view>
#include <system_error>

namespace
{

class stream_capture
{
public:
    explicit stream_capture(std::ostream &stream)
            : m_buffer()
            , m_stream(stream)
            , m_saved(stream.rdbuf(m_buffer.rdbuf()))
    {
    }

    ~stream_capture()
    {
        m_stream.rdbuf(m_saved);
    }

    stream_capture(const stream_capture &)            = delete;
    stream_capture &operator=(const stream_capture &) = delete;

    std::string str() const
    {
        return m_buffer.str();
    }

private:
    std::ostringstream m_buffer;
    std::ostream &m_stream;
    std::streambuf *m_saved;
};

struct outcome
{
    int code;
    std::string out;
    std::string err;
};

// The command line's environment layer reads both variables, so a run holds them at the value it
// names and leaves them empty, adding nothing to the next run's stack.
outcome flatten(const std::filesystem::path &document, const std::vector<std::string> &roots, const std::string &ros_package_path = {})
{
    meios::cli::verb_context ctx;
    ctx.id            = "flatten";
    ctx.positionals   = {document.string()};
    ctx.package_paths = roots;
    ros_test::portable_setenv("ROS_PACKAGE_PATH", ros_package_path.c_str());
    ros_test::portable_setenv("AMENT_PREFIX_PATH", "");
    stream_capture out(std::cout);
    stream_capture err(std::cerr);
    const outcome ran{meios::cli::run_flatten(ctx), out.str(), err.str()};
    ros_test::portable_setenv("ROS_PACKAGE_PATH", "");
    return ran;
}

bool contains(const std::string &text, std::string_view fragment)
{
    return text.find(fragment) != std::string::npos;
}

std::filesystem::path find_document(const include_tree::tree &held)
{
    held.write("R/p/package.xml", "<package><name>p</name></package>");
    return held.write("T/find.xacro", include_tree::robot("<link name=\"$(find p)\"/>"));
}

bool linked_directory(const std::filesystem::path &target, const std::filesystem::path &link)
{
    std::error_code ec;
    std::filesystem::create_directory_symlink(target, link, ec);
    return !ec;
}

}

TEST_CASE("cli_package_path: an include linking out of its package but inside the configured root renders through both package spellings")
{
    const include_tree::tree held;
    held.write("R/p/package.xml", "<package><name>p</name></package>");
    const std::filesystem::path target = held.write("R/q/a.xacro", include_tree::binds("in_root"));
    std::filesystem::create_directories(held.root() / "R/p/urdf");
    if(!include_tree::linked(target, held.root() / "R/p/urdf/a.xacro"))
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }

    for(const std::string_view spelling : {"$(find p)/urdf/a.xacro", "package://p/urdf/a.xacro"})
        DYNAMIC_SECTION(spelling)
        {
            const outcome ran = flatten(held.write("T/top.xacro", include_tree::reads_leaf(spelling)), {(held.root() / "R").string()});
            INFO(ran.err);
            CHECK(ran.code == 0);
            CHECK(contains(ran.out, "name=\"in_root\""));
            CHECK_FALSE(contains(ran.err, "[error]"));
        }
}

TEST_CASE("cli_package_path: a sibling spelled back through a linked directory refuses as the reference refuses it")
{
    const include_tree::tree held;
    held.write("R/p/urdf/top.xacro", include_tree::robot(include_tree::include("sub/../b.xacro")));
    held.write("R/p/urdf/a/b.xacro", include_tree::robot(include_tree::include("c.xacro")));
    held.write("R/p/urdf/b.xacro", include_tree::binds("lexical_b"));
    held.write("R/p/urdf/c.xacro", include_tree::binds("layer_lexical_c"));
    std::filesystem::create_directories(held.root() / "R/p/urdf/a/b");
    if(!include_tree::linked("a/b", held.root() / "R/p/urdf/sub"))
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }

    for(const std::string_view spelling : {"$(find p)/urdf/top.xacro", "package://p/urdf/top.xacro"})
        DYNAMIC_SECTION(spelling)
        {
            const outcome ran = flatten(held.write("T/top.xacro", include_tree::reads_leaf(spelling)), {(held.root() / "R").string()});
            INFO(ran.err);
            CHECK(ran.code != 0);
            CHECK_FALSE(contains(ran.out, "layer_lexical_c"));
            CHECK(contains(ran.err, "(unresolved_include)"));
            CHECK(contains(ran.err, "c.xacro"));
        }
}

TEST_CASE("cli_package_path: the find command through a linked root renders the root as it was configured")
{
    const include_tree::tree held;
    const std::filesystem::path document = find_document(held);
    if(!linked_directory(held.root() / "R", held.root() / "Rl"))
    {
        SUCCEED("this host cannot create a directory symlink");
        return;
    }

    const outcome linked = flatten(document, {(held.root() / "Rl").string()});
    const outcome dotted = flatten(document, {(held.root() / "." / "Rl").string()});
    const outcome direct = flatten(document, {(held.root() / "." / "R").string()});
    CHECK(contains(linked.out, (held.root() / "Rl" / "p").generic_string()));
    CHECK(contains(dotted.out, (held.root() / "Rl" / "p").generic_string()));
    CHECK(contains(direct.out, (held.root() / "R" / "p").generic_string()));
    CHECK_FALSE(contains(dotted.out + direct.out, "/./"));
}

TEST_CASE("cli_package_path: the find command through a linked ROS package path renders the crawled spelling")
{
    if(!meios::cli::ros_linked())
    {
        SUCCEED("meios::ros-package not linked; the ros resolution layer is absent");
        return;
    }
    const include_tree::tree held;
    const std::filesystem::path document = find_document(held);
    if(!linked_directory(held.root() / "R", held.root() / "Rl"))
    {
        SUCCEED("this host cannot create a directory symlink");
        return;
    }

    const outcome ran = flatten(document, {}, (held.root() / "Rl").string());
    INFO(ran.err);
    CHECK(ran.code == 0);
    CHECK(contains(ran.out, (held.root() / "Rl" / "p").generic_string()));
}

TEST_CASE("cli_package_path: a relative package path renders the find command's directory absolute")
{
    const include_tree::tree held;
    const std::filesystem::path document = find_document(held);
    outcome ran{};
    std::filesystem::path expected;
    {
        const include_tree::working_directory here(held.root());
        expected = std::filesystem::current_path() / "R" / "p";
        ran      = flatten(document, {"R"});
    }

    INFO(ran.err);
    CHECK(ran.code == 0);
    CHECK(expected.is_absolute());
    CHECK(contains(ran.out, "name=\"" + expected.generic_string() + '"'));
}
