#include "xacro_read_probe.h"
#include "xacro_include_tree.h"

#include <meios/xacro.h>

#include <meios/io/source_stack.h>
#include <meios/io/directory_source.h>

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <vector>
#include <filesystem>
#include <string_view>

using include_tree::robot;
using include_tree::binds;
using include_tree::linked;
using include_tree::include;
using include_tree::reads_leaf;
using include_tree::expand_file;
using include_tree::linked_robot;
using include_tree::renders_link;

namespace
{

class recording_fetch final : public meios::text_resource_loader::fetcher
{
public:
    explicit recording_fetch(std::vector<std::filesystem::path> &seen)
            : m_seen(seen)
    {
    }

    meios::text_resource_result fetch(std::string_view, const std::filesystem::path &document) override
    {
        m_seen.push_back(document);
        return meios::unexpected<meios::text_resource_failure>(meios::text_resource_failure::unreachable);
    }

private:
    std::vector<std::filesystem::path> &m_seen;
};

}

TEST_CASE("a cycle through a package include and beside includes refuses at its first re-entry", "[xacro][structural][include][identity][cycle]")
{
    const include_tree::tree held;
    held.write("root/p/a.xacro", robot(include("b.xacro")));
    held.write("root/p/b.xacro", robot(include("a.xacro")));
    const std::filesystem::path top = held.write("doc/top.xacro", robot(include("package://p/a.xacro")));
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{xacro_probe::recorder{records}};
    meios::source_stack sources{meios::directory_source{held.root() / "root", log}};

    const include_tree::expansion_result out = expand_file(top, sources, log);
    REQUIRE_FALSE(out.has_value());
    REQUIRE(out.error().code == meios::diagnostic_code::xacro_structural_error);
    REQUIRE(out.error().message.find(std::filesystem::weakly_canonical(held.root() / "root" / "p" / "a.xacro").string()) != std::string::npos);
    REQUIRE(out.error().loc.file.filename() == "b.xacro");
    REQUIRE(xacro_probe::errors(records) == 1);
}

TEST_CASE("an include found through the sources expands the same from any working directory", "[xacro][structural][include][identity]")
{
    const include_tree::tree held;
    held.write("root/n.xacro", robot(include("package://cwd/k.xacro")));
    held.write("root/cwd/k.xacro", robot(include("n.xacro")));
    held.write("root/cwd/n.xacro", binds("inner_n"));
    const std::filesystem::path top = held.write("doc/top.xacro", reads_leaf("n.xacro"));
    std::filesystem::create_directories(held.root() / "elsewhere");
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{xacro_probe::recorder{records}};
    meios::source_stack sources{meios::directory_source{held.root() / "root", log}};

    include_tree::expansion_result beside_same_name = [&]
    {
        const include_tree::working_directory here(held.root() / "root" / "cwd");
        return expand_file(top, sources, log);
    }();
    include_tree::expansion_result unrelated = [&]
    {
        const include_tree::working_directory here(held.root() / "elsewhere");
        return expand_file(top, sources, log);
    }();

    REQUIRE(renders_link(beside_same_name, "inner_n"));
    REQUIRE(renders_link(unrelated, "inner_n"));
    REQUIRE(beside_same_name->document == unrelated->document);
    REQUIRE(xacro_probe::errors(records) == 0);
}

TEST_CASE("a relative include in a linked document resolves beside the link", "[xacro][structural][include][identity][link]")
{
    const include_tree::tree held;
    held.write("install/gen.xacro", binds("generated"));
    held.write("src/gen.xacro", binds("source"));
    const std::filesystem::path document = linked_robot(held, include("gen.xacro") + "<link name=\"${leaf}\"/>");
    if(document.empty())
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{xacro_probe::recorder{records}};
    meios::source_stack sources;

    REQUIRE(renders_link(expand_file(document, sources, log), "generated"));
    REQUIRE(xacro_probe::errors(records) == 0);
}

TEST_CASE("a nested include in a linked sibling resolves beside the link", "[xacro][structural][include][identity][link]")
{
    const include_tree::tree held;
    held.write("install/real/a.xacro", robot(include("b.xacro")));
    held.write("install/b.xacro", binds("beside_link"));
    held.write("install/real/b.xacro", binds("beside_target"));
    const std::filesystem::path top = held.write("install/top.xacro", reads_leaf("a.xacro"));
    if(!linked("real/a.xacro", held.root() / "install" / "a.xacro"))
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{xacro_probe::recorder{records}};
    meios::source_stack sources;

    REQUIRE(renders_link(expand_file(top, sources, log), "beside_link"));
    REQUIRE(xacro_probe::errors(records) == 0);
}

TEST_CASE("an auxiliary document is resolved against a linked document's path as given", "[xacro][structural][include][identity][link]")
{
    const include_tree::tree held;
    const std::filesystem::path document = linked_robot(held, "<link name=\"${xacro.load_yaml('cfg.yaml')}\"/>");
    if(document.empty())
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }
    std::vector<std::filesystem::path> seen;
    meios::eval_scope scope;
    scope.install_text_loader(meios::text_resource_loader{std::make_unique<recording_fetch>(seen)});
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{xacro_probe::recorder{records}};
    meios::source_stack sources;

    meios::expand(include_tree::read_file(document), scope, sources, document, meios::expansion_limits{}, log);
    REQUIRE(seen.size() == 1);
    REQUIRE(seen.front() == document);
}

TEST_CASE("a relative include in a document under a linked directory resolves beside it", "[xacro][structural][include][identity][link]")
{
    const include_tree::tree held;
    held.write("src/urdf/gen.xacro", binds("generated"));
    held.write("src/urdf/robot.xacro", reads_leaf("gen.xacro"));
    std::filesystem::create_directories(held.root() / "install");
    if(!linked(held.root() / "src" / "urdf", held.root() / "install" / "urdf"))
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{xacro_probe::recorder{records}};
    meios::source_stack sources;

    REQUIRE(renders_link(expand_file(held.root() / "install" / "urdf" / "robot.xacro", sources, log), "generated"));
    REQUIRE(xacro_probe::errors(records) == 0);
}

TEST_CASE("the directory command names the located directory of the file it is written in", "[xacro][structural][include][identity][dirname]")
{
    const include_tree::tree held;
    const std::string names_directory = "<link name=\"$(dirname)\"/>";
    held.write("pkg/sub/inner.xacro", robot(names_directory));
    held.write("doc/sub2/beside.xacro", robot(names_directory));
    held.write("cwd/pkg/sub/inner.xacro", robot("<link name=\"working_directory\"/>"));
    const std::filesystem::path top = held.write("doc/top.xacro", robot(include("package://pkg/sub/inner.xacro") + include("sub2/beside.xacro") + names_directory));
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{xacro_probe::recorder{records}};
    meios::source_stack sources{meios::directory_source{held.root(), log}};
    std::filesystem::create_directories(held.root() / "elsewhere");
    const include_tree::expansion_result beside_same_name = include_tree::expand_file_from(held.root() / "cwd", top, sources, log);
    const include_tree::expansion_result unrelated        = include_tree::expand_file_from(held.root() / "elsewhere", top, sources, log);

    REQUIRE((beside_same_name.has_value() && unrelated.has_value()));
    REQUIRE(beside_same_name->document == unrelated->document);
    for(const std::filesystem::path &directory :
        {std::filesystem::weakly_canonical(held.root() / "pkg" / "sub"), std::filesystem::weakly_canonical(held.root() / "doc" / "sub2"), top.parent_path()})
        REQUIRE(renders_link(unrelated, directory.generic_string()));
    REQUIRE(xacro_probe::errors(records) == 0);
}
