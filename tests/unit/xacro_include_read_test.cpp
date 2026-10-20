#include "xacro_read_probe.h"
#include "xacro_include_tree.h"

#include <meios/xacro.h>

#include <meios/io/text_reader.h>
#include <meios/io/source_stack.h>
#include <meios/io/directory_source.h>

#include <meios/diagnostic/log_sink.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <system_error>

namespace
{

using expansion_result = meios::expected<meios::expansion, meios::expansion_error>;

expansion_result expand_over(const char *source, meios::source_stack &sources,
                             const std::filesystem::path &document, meios::log_sink &log)
{
    meios::eval_scope scope;
    return meios::expand(source, scope, sources, document, meios::expansion_limits{}, log);
}

}

TEST_CASE("an xacro:include naming a directory refuses with the non-regular classification",
          "[xacro][structural][include][read]")
{
    const xacro_probe::directory_target held{ "inc.xacro" };
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{ xacro_probe::recorder{ records } };
    meios::source_stack sources{ meios::directory_source{ held.tree.path(), log } };

    const char *top = "<robot xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
                      "<xacro:include filename=\"inc.xacro\"/></robot>";
    const expansion_result out =
        expand_over(top, sources, held.tree.path() / "top.xacro", log);

    REQUIRE_FALSE(out.has_value());
    REQUIRE(xacro_probe::coded(records, meios::diagnostic_code::unresolved_include) == 1);
    REQUIRE(xacro_probe::coded(records, meios::diagnostic_code::xacro_parse_error) == 0);
    REQUIRE(xacro_probe::errors(records) == 1);
    const xacro_probe::captured &refusal =
        *std::find_if(records.begin(), records.end(), [](const xacro_probe::captured &r) {
            return r.code == meios::diagnostic_code::unresolved_include;
        });
    REQUIRE(refusal.lvl == meios::level::error);
    REQUIRE(refusal.cause.has_value());
    REQUIRE(refusal.cause->operation == meios::operation_kind::status);
    // A refusal by classification carries no native code: the kind is the whole failure, and
    // the reader states so rather than reporting a cause whose message reads "Success".
    REQUIRE(refusal.cause->native == std::error_code{});
    const meios::text_read_failure failure = meios::read_text_file(held.target).error();
    REQUIRE(failure.kind == meios::text_read_failure_kind::non_regular);
}

TEST_CASE("an absent xacro:include target classifies apart from a directory one",
          "[xacro][structural][include][read]")
{
    const xacro_probe::directory_target held{ "other" };
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{ xacro_probe::recorder{ records } };
    meios::source_stack sources{ meios::directory_source{ held.tree.path(), log } };

    const char *top = "<robot xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
                      "<xacro:include filename=\"absent.xacro\"/></robot>";
    const expansion_result out =
        expand_over(top, sources, held.tree.path() / "top.xacro", log);

    REQUIRE_FALSE(out.has_value());
    REQUIRE(xacro_probe::coded(records, meios::diagnostic_code::unresolved_include) == 1);

    // The source layer refuses an absent target before any read, so the second classification
    // is driven at the reader: it answers a different kind and a real native code for an absent
    // path than for a directory, which is what proves the refusal reads the classification
    // rather than hard-coding one failure.
    const meios::text_read_failure absent =
        meios::read_text_file(held.tree.path() / "absent.xacro").error();
    REQUIRE(absent.kind == meios::text_read_failure_kind::open);
    REQUIRE(absent.kind != meios::read_text_file(held.target).error().kind);
    REQUIRE(absent.cause.native != std::error_code{});
}

TEST_CASE("an empty regular xacro:include is read and still reaches the parser",
          "[xacro][structural][include][read]")
{
    const xacro_probe::empty_target held{ "inc.xacro" };
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{ xacro_probe::recorder{ records } };
    meios::source_stack sources{ meios::directory_source{ held.tree.path(), log } };

    const char *top = "<robot xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
                      "<xacro:include filename=\"inc.xacro\"/></robot>";
    const expansion_result out =
        expand_over(top, sources, held.tree.path() / "top.xacro", log);

    REQUIRE_FALSE(out.has_value());
    REQUIRE(xacro_probe::coded(records, meios::diagnostic_code::unresolved_include) == 0);
    REQUIRE(xacro_probe::coded(records, meios::diagnostic_code::xacro_parse_error) == 1);
}

TEST_CASE("a package include of a link pointing inside its configured root is read through the root",
          "[xacro][structural][include][read]")
{
    const include_tree::tree held;
    held.write("root/p/real/a.xacro", include_tree::binds("leaf"));
    if(!include_tree::linked("real/a.xacro", held.root() / "root" / "p" / "a.xacro"))
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{ xacro_probe::recorder{ records } };
    meios::source_stack sources{ meios::directory_source{ held.root() / "root", log } };

    const expansion_result out = include_tree::expand_text(include_tree::reads_leaf("package://p/a.xacro"), held.root() / "doc" / "top.xacro", sources, log);
    REQUIRE(include_tree::renders_link(out, "leaf"));
    REQUIRE(xacro_probe::errors(records) == 0);
}

TEST_CASE("a package include of a link pointing out of its configured root refuses as uncontained",
          "[xacro][structural][include][read]")
{
    const include_tree::tree held;
    const std::filesystem::path target = held.write("elsewhere/a.xacro", include_tree::binds("leaf"));
    std::filesystem::create_directories(held.root() / "root" / "p");
    if(!include_tree::linked(target, held.root() / "root" / "p" / "a.xacro"))
    {
        SUCCEED("this host cannot create a symlink");
        return;
    }
    std::vector<xacro_probe::captured> records;
    meios::log_sink_f log{ xacro_probe::recorder{ records } };
    meios::source_stack sources{ meios::directory_source{ held.root() / "root", log } };

    const expansion_result out = include_tree::expand_text(include_tree::reads_leaf("package://p/a.xacro"), held.root() / "doc" / "top.xacro", sources, log);
    REQUIRE_FALSE(out.has_value());
    REQUIRE(out.error().code == meios::diagnostic_code::unresolved_include);
    REQUIRE(out.error().message == "xacro:include could not resolve \"p/a.xacro\"");
    REQUIRE(xacro_probe::coded(records, meios::diagnostic_code::uncontained_asset) == 1);
    REQUIRE(xacro_probe::errors(records) == 2);
}
