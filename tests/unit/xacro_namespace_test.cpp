#include "xacro_include_tree.h"

#include <meios/xacro.h>

#include <meios/xacro/evaluator_handle.h>

#include <meios/io/source_stack.h>

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <cstddef>
#include <optional>
#include <filesystem>
#include <string_view>

namespace
{

using include_tree::expansion_result;

std::string property(std::string_view name, std::string_view value)
{
    return "<xacro:property name=\"" + std::string(name) + "\" value=\"" + std::string(value) + "\"/>";
}

std::string include_as(std::string_view filename, std::string_view space)
{
    return "<xacro:include filename=\"" + std::string(filename) + "\" ns=\"" + std::string(space) + "\"/>";
}

expansion_result expand_in(meios::eval_scope &scope, const include_tree::tree &held, std::string_view body)
{
    const std::filesystem::path document = held.write("top.xacro", include_tree::robot(body));
    meios::log_sink silent;
    meios::source_stack sources{};
    return meios::expand(include_tree::read_file(document), scope, sources, document, meios::expansion_limits{}, silent);
}

expansion_result expand_top(const include_tree::tree &held, std::string_view body)
{
    meios::eval_scope scope;
    return expand_in(scope, held, body);
}

// Reads `space.member` through the public namespace query alone, as a custom evaluator would.
struct dotted_backend
{
    meios::text_outcome eval_to_text(std::string_view expr, const meios::eval_scope &scope, meios::log_sink &) const
    {
        const std::size_t dot = expr.find('.');
        const std::optional<meios::scope_namespace> space = scope.namespace_of(expr.substr(0, dot));
        const std::optional<meios::value> read = space ? scope.lookup(*space, expr.substr(dot + 1)) : std::nullopt;
        if(!read || !read->text())
            return meios::text_outcome{ std::nullopt, meios::eval_failure_kind::error };
        return meios::text_outcome{ *read->text(), meios::eval_failure_kind::none };
    }
};

bool refuses_with(const expansion_result &out, meios::diagnostic_code code, std::string_view naming)
{
    return !out.has_value() && out.error().code == code && out.error().message.find(naming) != std::string::npos;
}

}

TEST_CASE("a property of a namespaced include reads through the namespace and a dot", "[xacro][namespace]")
{
    const include_tree::tree held;
    held.write("w.xacro", include_tree::robot(property("w", "7")));
    REQUIRE(include_tree::renders_link(expand_top(held, include_as("w.xacro", "n") + "<link name=\"a${n.w}\"/>"), "a7"));
}

TEST_CASE("a property of a namespaced include is not visible by its bare name", "[xacro][namespace]")
{
    const include_tree::tree held;
    held.write("w.xacro", include_tree::robot(property("w", "7")));
    const expansion_result out = expand_top(held, include_as("w.xacro", "n") + "<link name=\"a${w}\"/>");
    REQUIRE(refuses_with(out, meios::diagnostic_code::undefined_property, "name 'w' is not defined"));
}

TEST_CASE("a name the namespace lacks is read through the table the include was read in", "[xacro][namespace]")
{
    const include_tree::tree held;
    held.write("n.xacro", include_tree::robot(property("y", "ny")));
    const expansion_result out = expand_top(held, property("x", "px") + include_as("n.xacro", "n") + "<link name=\"${n.x}\"/>");
    REQUIRE(include_tree::renders_link(out, "px"));
}

TEST_CASE("a namespaced property is evaluated in its namespace and sees a global written after the include",
          "[xacro][namespace]")
{
    const include_tree::tree held;
    held.write("n.xacro", include_tree::robot(property("lz", "a${g}")));
    const expansion_result out = expand_top(held, include_as("n.xacro", "n") + property("g", "late") + "<link name=\"${n.lz}\"/>");
    REQUIRE(include_tree::renders_link(out, "alate"));
}

TEST_CASE("a second include binding the same namespace replaces the first", "[xacro][namespace]")
{
    const include_tree::tree held;
    held.write("w.xacro", include_tree::robot(property("w", "1")));
    held.write("v.xacro", include_tree::robot(property("v", "2")));
    const std::string both = include_as("w.xacro", "n") + include_as("v.xacro", "n");
    REQUIRE(include_tree::renders_link(expand_top(held, both + "<link name=\"v${n.v}\"/>"), "v2"));
    const expansion_result out = expand_top(held, both + "<link name=\"w${n.w}\"/>");
    REQUIRE(refuses_with(out, meios::diagnostic_code::undefined_property, "name 'w' is not defined"));
}

TEST_CASE("an empty namespace reads the file into the including table", "[xacro][namespace]")
{
    const include_tree::tree held;
    held.write("w.xacro", include_tree::robot(property("w", "7")));
    REQUIRE(include_tree::renders_link(expand_top(held, include_as("w.xacro", "") + "<link name=\"a${w}\"/>"), "a7"));
}

TEST_CASE("a namespace is evaluated before the filename, so its refusal is the one reported", "[xacro][namespace]")
{
    const include_tree::tree held;
    const expansion_result out = expand_top(held, include_as("${no_file}.xacro", "${no_space}"));
    REQUIRE(refuses_with(out, meios::diagnostic_code::undefined_property, "no_space"));
}

TEST_CASE("a namespace an expansion opened is found by name only while it runs", "[xacro][namespace]")
{
    const include_tree::tree held;
    held.write("w.xacro", include_tree::robot(property("w", "7")));
    meios::eval_scope scope;
    REQUIRE(include_tree::renders_link(expand_in(scope, held, include_as("w.xacro", "n") + "<link name=\"a${n.w}\"/>"), "a7"));
    REQUIRE(scope.contains("n"));
    REQUIRE_FALSE(scope.namespace_of("n").has_value());
}

TEST_CASE("a namespace member read inside a macro body reads the namespace table, not the call's",
          "[xacro][namespace]")
{
    const include_tree::tree held;
    held.write("n.xacro", include_tree::robot(property("w", "7") + property("lz", "a${q}")));
    const std::string macro = "<xacro:macro name=\"m\" params=\"w q\"><link name=\"${n.w}_${w}_${n.lz}\"/></xacro:macro>";
    const expansion_result out = expand_top(held, include_as("n.xacro", "n") + property("q", "glob") + macro
                                                      + "<xacro:m w=\"cw\" q=\"cq\"/>");
    REQUIRE(include_tree::renders_link(out, "7_cw_aglob"));
}

TEST_CASE("a custom evaluator reads a namespace member, and one the namespace lacks, through the query",
          "[xacro][namespace]")
{
    const include_tree::tree held;
    held.write("w.xacro", include_tree::robot(property("w", "w7")));
    const std::filesystem::path document = held.write("top.xacro", include_tree::robot(property("x", "px") + include_as("w.xacro", "n")
                                                                                       + "<link name=\"${n.w}\"/><link name=\"${n.x}\"/>"));
    meios::log_sink silent;
    meios::eval_scope scope;
    meios::source_stack sources{};
    const std::shared_ptr<meios::evaluator_handle> backend = std::make_shared<meios::evaluator_handle>(dotted_backend{});
    const expansion_result out = meios::expand(include_tree::read_file(document), scope, sources, document, meios::expansion_limits{},
                                               meios::eval_policy::fail, backend, silent);
    REQUIRE(include_tree::renders_link(out, "w7"));
    REQUIRE(include_tree::renders_link(out, "px"));
}
