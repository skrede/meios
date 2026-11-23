#include "cli_verbs_fixture.h"

#include "verbs/verbs.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

using namespace meios;
using meios::cli::verb_context;

TEST_CASE("cli_verbs: bundle fails loudly on an unopenable path and writes nothing")
{
    verb_context ctx;
    ctx.id                    = "bundle";
    ctx.positionals           = {fixture("this_file_does_not_exist.urdf")};
    ctx.value_flags["--name"] = "mybundle";
    cout_capture out;
    REQUIRE(cli::run_bundle(ctx) != 0);
    REQUIRE(out.str().empty());
}

TEST_CASE("cli_verbs: deps fails loudly on an unopenable path and emits no stdout")
{
    verb_context ctx;
    ctx.id          = "deps";
    ctx.positionals = {fixture("this_file_does_not_exist.urdf")};
    cout_capture out;
    REQUIRE(cli::run_deps(ctx) != 0);
    REQUIRE(out.str().empty());
}

TEST_CASE("cli_verbs: deps exits zero when nothing is unresolved")
{
    verb_context ctx;
    ctx.id          = "deps";
    ctx.positionals = {fixture("branched_all_joints.urdf")};
    cout_capture out;
    REQUIRE(cli::run_deps(ctx) == 0);
}

TEST_CASE("cli_verbs: bundle requires --name")
{
    verb_context ctx;
    ctx.id          = "bundle";
    ctx.positionals = {fixture("branched_all_joints.urdf")};
    REQUIRE(cli::run_bundle(ctx) == 1);
}

TEST_CASE("cli_verbs: bundle succeeds with --name and no unresolved assets")
{
    const std::filesystem::path work = std::filesystem::temp_directory_path() / "meios_cli_bundle";
    std::filesystem::create_directories(work);
    const std::filesystem::path saved = std::filesystem::current_path();
    std::filesystem::current_path(work);

    verb_context ctx;
    ctx.id                    = "bundle";
    ctx.positionals           = {fixture("branched_all_joints.urdf")};
    ctx.value_flags["--name"] = "mybundle";
    const int code            = cli::run_bundle(ctx);

    std::filesystem::current_path(saved);
    std::filesystem::remove_all(work);
    REQUIRE(code == 0);
}
