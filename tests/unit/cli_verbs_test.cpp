#include "cli_verbs_fixture.h"

#include "app.h"
#include "verbs/verbs.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <fstream>
#include <ostream>
#include <filesystem>

using namespace meios;
using meios::cli::verb_context;

TEST_CASE("cli_verbs: --package-path binds one path and keeps the model positional")
{
    const std::string root        = MEIOS_URDF_FIXTURE_DIR;
    const std::string model       = fixture("branched_all_joints.urdf");
    std::vector<std::string> args = {"meios", "flatten", "--package-path", root, "--package-path", root, model};
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
    const std::string model       = fixture("args_doc.xacro");
    std::vector<std::string> args = {"meios", "flatten", model, "prefix:=front"};
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
    ctx.id          = "flatten";
    ctx.positionals = {fixture("branched_all_joints.urdf")};
    cout_capture out;
    REQUIRE(cli::run_flatten(ctx) == 0);
    REQUIRE(out.str().find("<robot") != std::string::npos);
}

TEST_CASE("cli_verbs: flatten fails loudly on an unopenable path and emits no stub")
{
    verb_context ctx;
    ctx.id          = "flatten";
    ctx.positionals = {fixture("this_file_does_not_exist.urdf")};
    cout_capture out;
    REQUIRE(cli::run_flatten(ctx) != 0);
    REQUIRE(out.str().find("<robot") == std::string::npos);
}

TEST_CASE("cli_verbs: flatten fails loudly on a malformed argument override and emits no stub")
{
    verb_context ctx;
    ctx.id            = "flatten";
    ctx.positionals   = {fixture("args_doc.xacro")};
    ctx.arg_overrides = {"prefix=front"};
    cout_capture out;
    REQUIRE(cli::run_flatten(ctx) != 0);
    REQUIRE(out.str().find("<robot") == std::string::npos);
}

TEST_CASE("cli_verbs: flatten resolves a nested folder!=name package through the ros layer")
{
    if(!cli::ros_linked())
    {
        SUCCEED("meios::ros-package not linked; the ros resolution layer is absent");
        return;
    }

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "meios_cli_ros_flatten";
    std::filesystem::remove_all(root);
    const std::filesystem::path pkg = root / "deep" / "nested_dir";
    std::filesystem::create_directories(pkg);
    std::ofstream(pkg / "package.xml") << "<package><name>rospkg</name></package>";
    std::ofstream(pkg / "parts.xacro") << "<robot xmlns:xacro=\"http://www.ros.org/wiki/xacro\"><link name=\"arm\"/>"
                                          "<joint name=\"j\" type=\"fixed\"><parent link=\"base\"/><child link=\"arm\"/></joint>"
                                          "</robot>";
    const std::filesystem::path top = root / "top.urdf.xacro";
    std::ofstream(top) << "<robot name=\"r\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
                          "<xacro:include filename=\"$(find rospkg)/parts.xacro\"/>"
                          "<link name=\"base\"/></robot>";

    verb_context ctx;
    ctx.id            = "flatten";
    ctx.positionals   = {top.string()};
    ctx.package_paths = {root.string()};

    cout_capture out;
    const int code            = cli::run_flatten(ctx);
    const std::string printed = out.str();
    std::filesystem::remove_all(root);

    REQUIRE(code == 0);
    REQUIRE(printed.find("<robot") != std::string::npos);
    REQUIRE(printed.find("arm") != std::string::npos);
}
