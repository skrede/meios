#include "app.h"
#include "verbs/verbs.h"

#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <fstream>
#include <ostream>
#include <sstream>
#include <utility>
#include <iostream>
#include <filesystem>
#include <system_error>

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

// The twin of cout_capture for stderr: verbs relay typed diagnostics through a
// log_sink_s bound to std::cerr, so a test swaps std::cerr's rdbuf to assert on the
// bytes a failing verb writes.
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

// Removes a seeded tree even when a REQUIRE aborts the case, so one failing row cannot
// leave the next run resolving against a directory it did not create.
class scoped_tree
{
public:
    explicit scoped_tree(std::filesystem::path root) : m_root(std::move(root)) {}

    ~scoped_tree()
    {
        std::error_code ec;
        std::filesystem::remove_all(m_root, ec);
    }

    scoped_tree(const scoped_tree &) = delete;
    scoped_tree &operator=(const scoped_tree &) = delete;

    const std::filesystem::path &path() const { return m_root; }

private:
    std::filesystem::path m_root;
};

// One tree serves both halves of the containment rule: a package root holding an asset,
// and a sibling directory no configured root covers.
std::filesystem::path seed_resolve_tree()
{
    const std::filesystem::path base =
        std::filesystem::temp_directory_path() / "meios_cli_resolve";
    std::filesystem::remove_all(base);
    std::filesystem::create_directories(base / "root" / "somepkg" / "meshes");
    std::filesystem::create_directories(base / "outside");
    std::ofstream(base / "root" / "somepkg" / "meshes" / "x.stl", std::ios::binary)
        << "solid x\nendsolid x\n";
    std::ofstream(base / "outside" / "stray.stl", std::ios::binary) << "solid y\nendsolid y\n";
    return base;
}

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

TEST_CASE("cli_verbs: flatten fails loudly on a malformed argument override and emits no stub")
{
    verb_context ctx;
    ctx.id = "flatten";
    ctx.positionals = { fixture("args_doc.xacro") };
    ctx.arg_overrides = { "prefix=front" };
    cout_capture out;
    REQUIRE(cli::run_flatten(ctx) != 0);
    REQUIRE(out.str().find("<robot") == std::string::npos);
}

TEST_CASE("cli_verbs: flatten fails loudly on an empty-key argument override")
{
    verb_context ctx;
    ctx.id = "flatten";
    ctx.positionals = { fixture("args_doc.xacro") };
    ctx.arg_overrides = { ":=front" };
    cout_capture out;
    REQUIRE(cli::run_flatten(ctx) != 0);
    REQUIRE(out.str().find("<robot") == std::string::npos);
}

TEST_CASE("cli_verbs: bundle fails loudly on an unopenable path and writes nothing")
{
    verb_context ctx;
    ctx.id = "bundle";
    ctx.positionals = { fixture("this_file_does_not_exist.urdf") };
    ctx.value_flags["--name"] = "mybundle";
    cout_capture out;
    REQUIRE(cli::run_bundle(ctx) != 0);
    REQUIRE(out.str().empty());
}

TEST_CASE("cli_verbs: deps fails loudly on an unopenable path and emits no stdout")
{
    verb_context ctx;
    ctx.id = "deps";
    ctx.positionals = { fixture("this_file_does_not_exist.urdf") };
    cout_capture out;
    REQUIRE(cli::run_deps(ctx) != 0);
    REQUIRE(out.str().empty());
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

TEST_CASE("cli_verbs: info exits nonzero and prints each failure's typed code exactly once")
{
    struct row
    {
        std::string file;
        diagnostic_code code;
    };
    const std::vector<row> rows = {
        { fixture("malformed_xml.urdf"), diagnostic_code::xml_parse_error },
        { fixture("cycle.urdf"), diagnostic_code::no_root_cycle },
        { fixture("this_file_does_not_exist.urdf"), diagnostic_code::cannot_open },
        { fixture("undefined_property.xacro"), diagnostic_code::undefined_property },
    };

    for(const row &r : rows)
    {
        verb_context ctx;
        ctx.id = "info";
        ctx.positionals = { r.file };
        cerr_capture err;
        const int code = cli::run_info(ctx);
        const std::string token = "(" + std::string(to_string(r.code)) + ")";
        REQUIRE(code != 0);
        REQUIRE(occurrences(err.str(), token) == 1);
    }
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

TEST_CASE("cli_verbs: resolve answers for every form the asset matrix accepts")
{
    const scoped_tree tree(seed_resolve_tree());
    const std::filesystem::path root = tree.path() / "root";
    const std::filesystem::path asset = root / "somepkg" / "meshes" / "x.stl";
    const std::vector<std::string> targets = { "package://somepkg/meshes/x.stl",
                                               "somepkg/meshes/x.stl", asset.string(),
                                               "file://" + asset.generic_string() };

    for(const std::string &target : targets)
    {
        verb_context ctx;
        ctx.id = "resolve";
        ctx.positionals = { (root / "doc.urdf").string(), target };
        ctx.package_paths = { root.string() };
        cout_capture out;
        const int code = cli::run_resolve(ctx);
        const std::string printed = out.str();
        INFO("target " << target);
        REQUIRE(code == 0);
        REQUIRE(printed == asset.string() + "\n");
    }
}

TEST_CASE("cli_verbs: resolve refuses every form the asset matrix refuses, under the library's code")
{
    const scoped_tree tree(seed_resolve_tree());
    const std::filesystem::path root = tree.path() / "root";

    struct row
    {
        std::string    target;
        diagnostic_code code;
        std::string    mentions;
    };
    const std::vector<row> rows = {
        { "http://example.invalid/arm.dae", diagnostic_code::unsupported_uri_scheme, "'http'" },
        { "model://somepkg/meshes/x.stl", diagnostic_code::unsupported_uri_scheme, "'model'" },
        { (tree.path() / "outside" / "stray.stl").string(), diagnostic_code::uncontained_asset,
          "stray.stl" },
        { "../outside/stray.stl", diagnostic_code::uncontained_asset, "../outside/stray.stl" },
        { "package://somepkg", diagnostic_code::malformed_asset_uri, "package://somepkg" },
        { "package://absent/mesh.stl", diagnostic_code::unresolved_asset, "absent" },
        { "somepkg/meshes/absent.stl", diagnostic_code::unresolved_asset, "absent.stl" },
    };

    for(const row &r : rows)
    {
        verb_context ctx;
        ctx.id = "resolve";
        ctx.positionals = { (root / "doc.urdf").string(), r.target };
        ctx.package_paths = { root.string() };
        cout_capture out;
        cerr_capture err;
        const int code = cli::run_resolve(ctx);
        const std::string printed = out.str();
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
    ctx.id = "resolve";
    ctx.positionals = { "package://somepkg/meshes/x.stl" };
    cout_capture out;
    cerr_capture err;
    const int code = cli::run_resolve(ctx);
    const std::string printed = out.str();
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
    const scoped_tree tree(seed_resolve_tree());
    const std::filesystem::path root = tree.path() / "root";

    verb_context ctx;
    ctx.id = "resolve";
    ctx.positionals = { (root / "doc.urdf").string(), "somepkg/meshes/x.stl" };
    cout_capture out;
    const int code = cli::run_resolve(ctx);
    const std::string printed = out.str();

    REQUIRE(code == 0);
    REQUIRE(printed == (root / "somepkg" / "meshes" / "x.stl").string() + "\n");
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
