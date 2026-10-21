#include "xacro_read_probe.h"
#include "xacro_include_tree.h"
#include "load_include_options_fixture.h"

#include <meios/urdf.h>
#include <meios/xacro.h>

#include <meios/io/source_stack.h>
#include <meios/io/bundle_source.h>
#include <meios/io/directory_source.h>

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <filesystem>
#include <initializer_list>

using include_options_fixture::climbing;
using include_options_fixture::expand_with;
using include_options_fixture::served_climb;
using include_options_fixture::expand_top_with;
using include_options_fixture::parent_directory_probe;

namespace
{

using load_outcome = meios::expected<meios::load_result, meios::load_error>;

const std::string top_a = include_tree::include("package://pkg/urdf/a.xacro");

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

TEST_CASE("the include options default to refusing an outside link", "[urdf][xacro][include][options]")
{
    REQUIRE_FALSE(meios::include_options{}.follow_outside_links);
    REQUIRE_FALSE(meios::load_options{}.includes.follow_outside_links);
}

TEST_CASE("a climb that stays inside the document's directory takes the ordinary lookups with the climb admitted", "[urdf][xacro][include][options]")
{
    const include_tree::tree held;
    held.write("root/x.xacro", include_tree::binds("from_root"));
    const std::filesystem::path top = held.write("doc/top.xacro", include_tree::reads_leaf("sub/../x.xacro"));
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{ xacro_probe::recorder{ records } };
    meios::source_stack sources{ meios::directory_source{ held.root() / "root", log } };

    REQUIRE(include_tree::renders_link(expand_with(top, sources, log, meios::include_options{}), "from_root"));
    REQUIRE(include_tree::renders_link(expand_with(top, sources, log, climbing()), "from_root"));
    REQUIRE(xacro_probe::errors(records) == 0);
}

TEST_CASE("a served document's climb that stays inside its directory resolves through its layer with the climb admitted", "[urdf][xacro][include][options]")
{
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{ xacro_probe::recorder{ records } };
    meios::source_stack unset{ served_climb(log, "sub/../b.xacro", "pkg", "urdf/b.xacro", "sibling") };
    meios::source_stack admitted{ served_climb(log, "sub/../b.xacro", "pkg", "urdf/b.xacro", "sibling") };

    REQUIRE(include_tree::renders_link(expand_top_with(top_a, unset, log, meios::include_options{}), "sibling"));
    REQUIRE(include_tree::renders_link(expand_top_with(top_a, admitted, log, climbing()), "sibling"));
    REQUIRE(xacro_probe::errors(records) == 0);
}

TEST_CASE("a served document's climb inside its package resolves through its layer whatever was included first", "[urdf][xacro][include][options]")
{
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{ xacro_probe::recorder{ records } };
    meios::source_stack alone{ served_climb(log, "../common.xacro", "pkg", "common.xacro", "common") };
    meios::source_stack after{ served_climb(log, "../common.xacro", "pkg", "common.xacro", "common") };

    const include_tree::expansion_result cold = expand_top_with(top_a, alone, log, climbing());
    const include_tree::expansion_result warm = expand_top_with(include_tree::include("package://pkg/common.xacro") + top_a, after, log, climbing());
    REQUIRE(include_tree::renders_link(cold, "common"));
    REQUIRE(include_tree::renders_link(warm, "common"));
    REQUIRE(cold->document == warm->document);
}

TEST_CASE("a served document's climb out of its package refuses whatever was included first", "[urdf][xacro][include][options]")
{
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{ xacro_probe::recorder{ records } };
    meios::source_stack alone{ served_climb(log, "../../other/x.xacro", "other", "x.xacro", "other") };
    meios::source_stack after{ served_climb(log, "../../other/x.xacro", "other", "x.xacro", "other") };

    const include_tree::expansion_result cold = expand_top_with(top_a, alone, log, climbing());
    const include_tree::expansion_result warm = expand_top_with(include_tree::include("package://other/x.xacro") + top_a, after, log, climbing());
    for(const include_tree::expansion_result &out : { cold, warm })
    {
        REQUIRE_FALSE(out.has_value());
        REQUIRE(out.error().code == meios::diagnostic_code::xacro_structural_error);
        REQUIRE(out.error().message.find("\"../../other/x.xacro\"") != std::string::npos);
        REQUIRE(out.error().message.find("\"pkg\"") != std::string::npos);
    }
}

TEST_CASE("a bundle-served document's admitted climb reads where the operating system resolves it", "[urdf][xacro][include][options]")
{
    const include_tree::tree held;
    held.write("root/pkg/urdf/a.xacro", include_tree::robot(include_tree::include("../../other/x.xacro")));
    held.write("root/other/x.xacro", include_tree::binds("other"));
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{ xacro_probe::recorder{ records } };
    meios::source_stack sources{ meios::bundle_source{ held.root() / "root", log } };

    REQUIRE(include_tree::renders_link(expand_top_with(top_a, sources, log, climbing()), "other"));
    REQUIRE(xacro_probe::errors(records) == 0);
}

TEST_CASE("a climb with an over-long name names where it looked, a failed look on POSIX and a miss on Windows", "[urdf][xacro][include][options]")
{
    const include_tree::tree held;
    const std::filesystem::path top = held.write("doc/top.xacro", include_tree::reads_leaf("../" + std::string(300, 'n') + ".xacro"));
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{ xacro_probe::recorder{ records } };
    meios::source_stack sources;

    const include_tree::expansion_result out = expand_with(top, sources, log, climbing());
    REQUIRE_FALSE(out.has_value());
    REQUIRE(out.error().code == meios::diagnostic_code::unresolved_include);
    REQUIRE(out.error().message.find("beside") == std::string::npos);
    REQUIRE(out.error().message.find(" at \"") != std::string::npos);
#ifdef _WIN32
    REQUIRE(out.error().message.starts_with("xacro:include could not resolve \""));
    REQUIRE_FALSE(out.error().cause.has_value());
#else
    REQUIRE(out.error().message.starts_with("xacro:include could not look for \""));
    REQUIRE(out.error().cause.has_value());
#endif
}
