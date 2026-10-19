#include "xacro_read_probe.h"
#include "xacro_include_tree.h"

#include <meios/urdf.h>
#include <meios/xacro.h>

#include <meios/io/source_stack.h>
#include <meios/io/directory_source.h>

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <filesystem>
#include <system_error>

namespace
{

using load_outcome = meios::expected<meios::load_result, meios::load_error>;

std::filesystem::path parent_directory_probe()
{
    return std::filesystem::path{ MEIOS_XACRO_PROBE_DIR } / "include_parent_directory.xacro";
}

meios::include_options climbing()
{
    meios::include_options includes;
    includes.climb_above_document = true;
    return includes;
}

include_tree::expansion_result expand_with(const std::filesystem::path &document, meios::source_stack &sources, meios::log_sink &log, const meios::include_options &includes)
{
    meios::eval_scope scope;
    return meios::expand(include_tree::read_file(document), scope, sources, document, meios::expansion_limits{}, meios::eval_policy::fail, {}, log, includes);
}

}

TEST_CASE("the include options default to refusing a climb", "[urdf][xacro][include][options]")
{
    REQUIRE_FALSE(meios::include_options{}.climb_above_document);
    REQUIRE_FALSE(meios::load_options{}.includes.climb_above_document);
}

TEST_CASE("the load options carry the include options into the expansion", "[urdf][xacro][include][options]")
{
    const load_outcome refused = meios::load(parent_directory_probe());
    REQUIRE_FALSE(refused.has_value());
    REQUIRE(refused.error().code == meios::diagnostic_code::xacro_structural_error);

    meios::load_options opts;
    opts.includes.climb_above_document = true;
    const load_outcome admitted = meios::load(parent_directory_probe(), opts);
    REQUIRE(admitted.has_value());
    REQUIRE(admitted->robot.links.size() == 1);
    REQUIRE(admitted->robot.links[0].name == "parent_leaf");
}

TEST_CASE("a caller admitting a climb reads an include above the document's directory", "[urdf][xacro][include][options]")
{
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{ xacro_probe::recorder{ records } };
    meios::source_stack sources;

    REQUIRE(include_tree::renders_link(expand_with(parent_directory_probe(), sources, log, climbing()), "parent_leaf"));
    REQUIRE(xacro_probe::errors(records) == 0);
}

TEST_CASE("an admitted climb that names nothing refuses and consults no source", "[urdf][xacro][include][options]")
{
    const include_tree::tree held;
    held.write("root/missing.xacro", include_tree::binds("from_root"));
    const std::filesystem::path inner = held.write("doc/sub/inner.xacro", include_tree::reads_leaf("../missing.xacro"));
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{ xacro_probe::recorder{ records } };
    meios::source_stack sources{ meios::directory_source{ held.root() / "root", log } };

    const include_tree::expansion_result out = expand_with(inner, sources, log, climbing());
    REQUIRE_FALSE(out.has_value());
    REQUIRE(out.error().code == meios::diagnostic_code::unresolved_include);
    REQUIRE_FALSE(include_tree::renders_link(out, "from_root"));
    REQUIRE(xacro_probe::errors(records) == 1);
}

TEST_CASE("an admitted climb resolves through a linked directory as the operating system does", "[urdf][xacro][include][options]")
{
    const include_tree::tree held;
    held.write("doc/x.xacro", include_tree::binds("lexical"));
    held.write("elsewhere/x.xacro", include_tree::binds("through_link"));
    std::filesystem::create_directories(held.root() / "elsewhere" / "d");
    const std::filesystem::path top = held.write("doc/top.xacro", include_tree::reads_leaf("sub/../x.xacro"));
    std::error_code ec;
    std::filesystem::create_directory_symlink(held.root() / "elsewhere" / "d", held.root() / "doc" / "sub", ec);
    if(ec)
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }
    const std::string opened = include_tree::read_file(held.root() / "doc" / "sub/../x.xacro");
    const std::string expected = opened.find("through_link") != std::string::npos ? "through_link" : "lexical";
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{ xacro_probe::recorder{ records } };
    meios::source_stack sources;

    REQUIRE(include_tree::renders_link(expand_with(top, sources, log, meios::include_options{}), "lexical"));
    REQUIRE(include_tree::renders_link(expand_with(top, sources, log, climbing()), expected));
    REQUIRE(xacro_probe::errors(records) == 0);
}

TEST_CASE("a climbing spelling with a package refuses with the climb admitted", "[urdf][xacro][include][options]")
{
    const include_tree::tree held;
    held.write("root/x.xacro", include_tree::binds("from_root"));
    const std::filesystem::path top = held.write("doc/top.xacro", include_tree::reads_leaf("package://pkg/../x.xacro"));
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{ xacro_probe::recorder{ records } };
    meios::source_stack sources{ meios::directory_source{ held.root() / "root", log } };

    const include_tree::expansion_result unset = expand_with(top, sources, log, meios::include_options{});
    const include_tree::expansion_result admitted = expand_with(top, sources, log, climbing());
    REQUIRE_FALSE(unset.has_value());
    REQUIRE(unset.error().code == meios::diagnostic_code::xacro_structural_error);
    REQUIRE_FALSE(admitted.has_value());
    REQUIRE(admitted.error().code == meios::diagnostic_code::xacro_structural_error);
}
