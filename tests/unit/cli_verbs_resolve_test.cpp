#include "cli_verbs_fixture.h"

#include "verbs/verbs.h"

#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <fstream>
#include <ostream>
#include <utility>
#include <filesystem>
#include <system_error>

using namespace meios;
using meios::cli::verb_context;

namespace
{

// Removes a seeded tree even when a REQUIRE aborts the case, so one failing row cannot
// leave the next run resolving against a directory it did not create.
class scoped_tree
{
public:
    explicit scoped_tree(std::filesystem::path root)
            : m_root(std::move(root))
    {
    }

    ~scoped_tree()
    {
        std::error_code ec;
        std::filesystem::remove_all(m_root, ec);
    }

    scoped_tree(const scoped_tree &)            = delete;
    scoped_tree &operator=(const scoped_tree &) = delete;

    const std::filesystem::path &path() const
    {
        return m_root;
    }

private:
    std::filesystem::path m_root;
};

// One tree serves both halves of the containment rule: a package root holding an asset,
// and a sibling directory no configured root covers.
std::filesystem::path seed_resolve_tree(const std::string &name)
{
    const std::filesystem::path base = std::filesystem::temp_directory_path() / name;
    std::filesystem::remove_all(base);
    std::filesystem::create_directories(base / "root" / "somepkg" / "meshes");
    std::filesystem::create_directories(base / "outside");
    std::ofstream(base / "root" / "somepkg" / "meshes" / "x.stl", std::ios::binary) << "solid x\nendsolid x\n";
    std::ofstream(base / "outside" / "stray.stl", std::ios::binary) << "solid y\nendsolid y\n";
    return base;
}

}

TEST_CASE("cli_verbs: resolve answers for every form the asset matrix accepts")
{
    const scoped_tree tree(seed_resolve_tree("meios_cli_resolve_accepts"));
    const std::filesystem::path root  = tree.path() / "root";
    const std::filesystem::path asset = root / "somepkg" / "meshes" / "x.stl";
    std::error_code error;
    const std::filesystem::path expected = std::filesystem::weakly_canonical(asset, error);
    REQUIRE_FALSE(error);
    const std::vector<std::string> targets = {"package://somepkg/meshes/x.stl", "somepkg/meshes/x.stl", asset.string(), "file://" + asset.generic_string()};

    for(const std::string &target : targets)
    {
        verb_context ctx;
        ctx.id            = "resolve";
        ctx.positionals   = {(root / "doc.urdf").string(), target};
        ctx.package_paths = {root.string()};
        cout_capture out;
        const int code            = cli::run_resolve(ctx);
        const std::string printed = out.str();
        INFO("target " << target);
        REQUIRE(code == 0);
        REQUIRE(printed == expected.string() + "\n");
    }
}

TEST_CASE("cli_verbs: resolve refuses every form the asset matrix refuses, under the library's code")
{
    const scoped_tree tree(seed_resolve_tree("meios_cli_resolve_refuses"));
    const std::filesystem::path root = tree.path() / "root";

    struct row
    {
        std::string target;
        diagnostic_code code;
        std::string mentions;
    };
    const std::vector<row> rows = {
            {"http://example.invalid/arm.dae", diagnostic_code::unsupported_uri_scheme, "'http'"},
            {"model://somepkg/meshes/x.stl", diagnostic_code::unsupported_uri_scheme, "'model'"},
            {(tree.path() / "outside" / "stray.stl").string(), diagnostic_code::uncontained_asset, "stray.stl"},
            {"../outside/stray.stl", diagnostic_code::uncontained_asset, "../outside/stray.stl"},
            {"package://somepkg", diagnostic_code::malformed_asset_uri, "package://somepkg"},
            {"package://absent/mesh.stl", diagnostic_code::unresolved_asset, "absent"},
            {"somepkg/meshes/absent.stl", diagnostic_code::unresolved_asset, "absent.stl"},
    };

    for(const row &r : rows)
    {
        verb_context ctx;
        ctx.id            = "resolve";
        ctx.positionals   = {(root / "doc.urdf").string(), r.target};
        ctx.package_paths = {root.string()};
        cout_capture out;
        cerr_capture err;
        const int code             = cli::run_resolve(ctx);
        const std::string printed  = out.str();
        const std::string reported = err.str();
        INFO("target " << r.target);
        REQUIRE(code != 0);
        REQUIRE(printed.empty());
        REQUIRE(occurrences(reported, "(" + std::string(to_string(r.code)) + ")") == 1);
        REQUIRE(reported.find(r.mentions) != std::string::npos);
    }
}

TEST_CASE("cli_verbs: resolve names the argument it is missing rather than resolving an empty one")
{
    verb_context ctx;
    ctx.id          = "resolve";
    ctx.positionals = {"package://somepkg/meshes/x.stl"};
    cout_capture out;
    cerr_capture err;
    const int code             = cli::run_resolve(ctx);
    const std::string printed  = out.str();
    const std::string reported = err.str();

    REQUIRE(code != 0);
    REQUIRE(printed.empty());
    REQUIRE(reported.find("asset reference") != std::string::npos);
    REQUIRE(reported.find("could not resolve asset ''") == std::string::npos);
}

// A bare relative target carries no base of its own, and the verb takes the same model
// positional every other verb does, so the base is the document's directory exactly as it is
// for a reference written inside that document. No package root is configured here, so the
// document's own directory is the only thing that can contain the asset.
TEST_CASE("cli_verbs: resolve measures a relative target against the model document's directory")
{
    const scoped_tree tree(seed_resolve_tree("meios_cli_resolve_relative"));
    const std::filesystem::path root  = tree.path() / "root";
    const std::filesystem::path asset = root / "somepkg" / "meshes" / "x.stl";
    std::error_code error;
    const std::filesystem::path expected = std::filesystem::weakly_canonical(asset, error);
    REQUIRE_FALSE(error);

    verb_context ctx;
    ctx.id          = "resolve";
    ctx.positionals = {(root / "doc.urdf").string(), "somepkg/meshes/x.stl"};
    cout_capture out;
    const int code            = cli::run_resolve(ctx);
    const std::string printed = out.str();

    REQUIRE(code == 0);
    REQUIRE(printed == expected.string() + "\n");
}
