#include "app.h"
#include "verbs/verbs.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <fstream>
#include <ostream>
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

// Redirects std::cout for the scope of one verb call so a test can assert on the
// bytes a verb writes, restoring the stream on destruction.
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

}

TEST_CASE("cli_verbs: --package-path binds one path and keeps the model positional")
{
    const std::string root = MEIOS_URDF_FIXTURE_DIR;
    const std::string model = fixture("branched_all_joints.urdf");
    std::vector<std::string> args = { "meios", "flatten", "--package-path", root,
                                      "--package-path", root, model };
    std::vector<char *> argv;
    for(std::string &arg : args)
        argv.push_back(arg.data());

    cout_capture out;
    const int code = cli::run(static_cast<int>(argv.size()), argv.data());
    REQUIRE(code == 0);
    REQUIRE(out.str().find("<robot") != std::string::npos);
}

TEST_CASE("cli_verbs: a trailing key:=value overrides a declared arg default")
{
    const std::string model = fixture("args_doc.xacro");
    std::vector<std::string> args = { "meios", "flatten", model, "prefix:=front" };
    std::vector<char *> argv;
    for(std::string &arg : args)
        argv.push_back(arg.data());

    cout_capture out;
    const int code = cli::run(static_cast<int>(argv.size()), argv.data());
    REQUIRE(code == 0);
    REQUIRE(out.str().find("frontbase_link") != std::string::npos);
}

TEST_CASE("cli_verbs: flatten prints a resolved robot document")
{
    verb_context ctx;
    ctx.id = "flatten";
    ctx.positionals = { fixture("branched_all_joints.urdf") };
    cout_capture out;
    REQUIRE(cli::run_flatten(ctx) == 0);
    REQUIRE(out.str().find("<robot") != std::string::npos);
}

TEST_CASE("cli_verbs: flatten fails loudly on an unopenable path and emits no stub")
{
    verb_context ctx;
    ctx.id = "flatten";
    ctx.positionals = { fixture("this_file_does_not_exist.urdf") };
    cout_capture out;
    REQUIRE(cli::run_flatten(ctx) != 0);
    REQUIRE(out.str().find("<robot") == std::string::npos);
}

TEST_CASE("cli_verbs: info summarizes links, joints, and degrees of freedom")
{
    verb_context ctx;
    ctx.id = "info";
    ctx.positionals = { fixture("branched_all_joints.urdf") };
    cout_capture out;
    REQUIRE(cli::run_info(ctx) == 0);
    REQUIRE(out.str().find("dof:") != std::string::npos);
}

TEST_CASE("cli_verbs: info --format json emits a machine-readable summary")
{
    verb_context ctx;
    ctx.id = "info";
    ctx.positionals = { fixture("branched_all_joints.urdf") };
    ctx.value_flags["--format"] = "json";
    cout_capture out;
    REQUIRE(cli::run_info(ctx) == 0);
    const std::string printed = out.str();
    REQUIRE(printed.find("\"name\":\"branched_all_joints\"") != std::string::npos);
    REQUIRE(printed.find("\"joints_by_kind\":") != std::string::npos);
    REQUIRE(printed.find("dof:") == std::string::npos);
}

TEST_CASE("cli_verbs: info rejects an unknown --format value")
{
    verb_context ctx;
    ctx.id = "info";
    ctx.positionals = { fixture("branched_all_joints.urdf") };
    ctx.value_flags["--format"] = "yaml";
    cout_capture out;
    REQUIRE(cli::run_info(ctx) != 0);
    REQUIRE(out.str().empty());
}

TEST_CASE("cli_verbs: args exits zero over a document")
{
    verb_context ctx;
    ctx.id = "args";
    ctx.positionals = { fixture("branched_all_joints.urdf") };
    cout_capture out;
    REQUIRE(cli::run_args(ctx) == 0);
}

TEST_CASE("cli_verbs: deps exits zero when nothing is unresolved")
{
    verb_context ctx;
    ctx.id = "deps";
    ctx.positionals = { fixture("branched_all_joints.urdf") };
    cout_capture out;
    REQUIRE(cli::run_deps(ctx) == 0);
}

TEST_CASE("cli_verbs: bundle requires --name")
{
    verb_context ctx;
    ctx.id = "bundle";
    ctx.positionals = { fixture("branched_all_joints.urdf") };
    REQUIRE(cli::run_bundle(ctx) == 1);
}

TEST_CASE("cli_verbs: bundle succeeds with --name and no unresolved assets")
{
    const std::filesystem::path work = std::filesystem::temp_directory_path() / "meios_cli_bundle";
    std::filesystem::create_directories(work);
    const std::filesystem::path saved = std::filesystem::current_path();
    std::filesystem::current_path(work);

    verb_context ctx;
    ctx.id = "bundle";
    ctx.positionals = { fixture("branched_all_joints.urdf") };
    ctx.value_flags["--name"] = "mybundle";
    const int code = cli::run_bundle(ctx);

    std::filesystem::current_path(saved);
    std::filesystem::remove_all(work);
    REQUIRE(code == 0);
}

TEST_CASE("cli_verbs: resolve rejects a malformed target")
{
    verb_context ctx;
    ctx.id = "resolve";
    ctx.positionals = { "", "not-a-reference" };
    REQUIRE(cli::run_resolve(ctx) == 1);
}

TEST_CASE("cli_verbs: resolve fails when a reference does not resolve")
{
    verb_context ctx;
    ctx.id = "resolve";
    ctx.positionals = { "", "package://absent/mesh.stl" };
    REQUIRE(cli::run_resolve(ctx) == 1);
}

TEST_CASE("cli_verbs: resolve locates a reference through a package root")
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "meios_cli_resolve";
    std::filesystem::create_directories(root / "somepkg" / "meshes");
    {
        std::ofstream asset(root / "somepkg" / "meshes" / "x.stl", std::ios::binary);
        asset << "solid x\nendsolid x\n";
    }
    verb_context ctx;
    ctx.id = "resolve";
    ctx.positionals = { "", "package://somepkg/meshes/x.stl" };
    ctx.package_paths = { root.string() };

    cout_capture out;
    const int code = cli::run_resolve(ctx);
    const std::string printed = out.str();
    std::filesystem::remove_all(root);

    REQUIRE(code == 0);
    REQUIRE(printed.find("x.stl") != std::string::npos);
}

TEST_CASE("cli_verbs: flatten resolves a nested folder!=name package through the ros layer")
{
    if(!cli::ros_linked())
    {
        SUCCEED("meios::ros not linked; the ros resolution layer is absent");
        return;
    }

    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "meios_cli_ros_flatten";
    std::filesystem::remove_all(root);
    const std::filesystem::path pkg = root / "deep" / "nested_dir";
    std::filesystem::create_directories(pkg);
    std::ofstream(pkg / "package.xml") << "<package><name>rospkg</name></package>";
    std::ofstream(pkg / "parts.xacro")
        << "<robot xmlns:xacro=\"http://www.ros.org/wiki/xacro\"><link name=\"arm\"/>"
           "<joint name=\"j\" type=\"fixed\"><parent link=\"base\"/><child link=\"arm\"/></joint>"
           "</robot>";
    const std::filesystem::path top = root / "top.urdf.xacro";
    std::ofstream(top) << "<robot name=\"r\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
                          "<xacro:include filename=\"$(find rospkg)/parts.xacro\"/>"
                          "<link name=\"base\"/></robot>";

    verb_context ctx;
    ctx.id = "flatten";
    ctx.positionals = { top.string() };
    ctx.package_paths = { root.string() };

    cout_capture out;
    const int code = cli::run_flatten(ctx);
    const std::string printed = out.str();
    std::filesystem::remove_all(root);

    REQUIRE(code == 0);
    REQUIRE(printed.find("<robot") != std::string::npos);
    REQUIRE(printed.find("arm") != std::string::npos);
}

TEST_CASE("cli_verbs: completion bash equals the golden and calls __complete")
{
    verb_context ctx;
    ctx.id = "completion";
    ctx.positionals = { "bash" };
    cout_capture out;
    REQUIRE(cli::run_completion(ctx) == 0);
    const std::string script = out.str();
    REQUIRE(script.find("meios __complete") != std::string::npos);
    REQUIRE(script == read_golden("completion/meios.bash"));
}

TEST_CASE("cli_verbs: completion rejects an unknown shell")
{
    verb_context ctx;
    ctx.id = "completion";
    ctx.positionals = { "powershell" };
    cout_capture out;
    REQUIRE(cli::run_completion(ctx) == 1);
}
