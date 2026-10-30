#include "xacro_include_tree.h"

#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <cstddef>
#include <filesystem>
#include <string_view>

namespace
{

using include_tree::expansion_result;

std::string property(std::string_view name, std::string_view value, std::string_view scope = "")
{
    const std::string scoped = scope.empty() ? "" : " scope=\"" + std::string(scope) + '"';
    return "<xacro:property name=\"" + std::string(name) + "\" value=\"" + std::string(value) + '"' + scoped + "/>";
}

std::string include_as(std::string_view filename, std::string_view space)
{
    return "<xacro:include filename=\"" + std::string(filename) + "\" ns=\"" + std::string(space) + "\"/>";
}

std::string macro(std::string_view name, std::string_view body)
{
    return "<xacro:macro name=\"" + std::string(name) + "\" params=\"\">" + std::string(body) + "</xacro:macro>";
}

expansion_result expand_top(const include_tree::tree &held, std::string_view body,
                            meios::eval_policy policy = meios::eval_policy::fail)
{
    const std::filesystem::path document = held.write("top.xacro", include_tree::robot(body));
    meios::log_sink silent;
    meios::eval_scope scope;
    meios::source_stack sources{};
    return meios::expand(include_tree::read_file(document), scope, sources, document, meios::expansion_limits{}, policy,
                         nullptr, silent);
}

bool refuses_with(const expansion_result &out, meios::diagnostic_code code, std::string_view naming)
{
    return !out.has_value() && out.error().code == code && out.error().message.find(naming) != std::string::npos;
}

std::size_t error_records(const include_tree::tree &held, std::string_view body, meios::eval_policy policy)
{
    const std::filesystem::path document = held.write("top.xacro", include_tree::robot(body));
    std::size_t errors = 0;
    meios::log_sink_f counting([&errors](meios::level lvl, const auto &...) { errors += lvl == meios::level::error ? 1 : 0; });
    meios::eval_scope scope;
    meios::source_stack sources{};
    const expansion_result out = meios::expand(include_tree::read_file(document), scope, sources, document,
                                               meios::expansion_limits{}, policy, nullptr, counting);
    return out.has_value() ? 0 : errors;
}

}

TEST_CASE("a macro of a namespaced include is called through the namespace and a dot", "[xacro][namespace]")
{
    const include_tree::tree held;
    held.write("w.xacro", include_tree::robot(macro("m", "<link name=\"nm\"/>")));
    REQUIRE(include_tree::renders_link(expand_top(held, include_as("w.xacro", "n") + "<xacro:n.m/>"), "nm"));
}

TEST_CASE("a namespaced macro reads its namespace's names bare and the including file's globals", "[xacro][namespace]")
{
    const include_tree::tree held;
    held.write("n.xacro", include_tree::robot(property("w", "7") + macro("m", "<link name=\"${w}_${g}\"/>")));
    const expansion_result out = expand_top(held, property("g", "glob") + include_as("n.xacro", "n") + "<xacro:n.m/>");
    REQUIRE(include_tree::renders_link(out, "7_glob"));
}

TEST_CASE("a macro of a namespaced file cannot call a macro of the including file", "[xacro][namespace]")
{
    const include_tree::tree held;
    held.write("n.xacro", include_tree::robot("<xacro:outer/>"));
    const expansion_result out = expand_top(held, macro("outer", "<link name=\"out\"/>") + include_as("n.xacro", "n"));
    REQUIRE(refuses_with(out, meios::diagnostic_code::xacro_structural_error, "xacro:outer"));
}

TEST_CASE("a macro is called and a property read through two nested namespaces", "[xacro][namespace]")
{
    const include_tree::tree held;
    held.write("b.xacro", include_tree::robot(property("c", "lC") + macro("m", "<link name=\"inC\"/>")));
    held.write("a.xacro", include_tree::robot(include_as("b.xacro", "b")));
    const std::string both = include_as("a.xacro", "a");
    REQUIRE(include_tree::renders_link(expand_top(held, both + "<xacro:a.b.m/>"), "inC"));
    REQUIRE(include_tree::renders_link(expand_top(held, both + "<link name=\"${a.b.c}\"/>"), "lC"));
}

TEST_CASE("a parent-scoped property in a namespaced macro skips the namespace to reach the caller", "[xacro][namespace]")
{
    const include_tree::tree held;
    held.write("n.xacro", include_tree::robot(macro("m", property("s", "S", "parent"))));
    const expansion_result out = expand_top(held, include_as("n.xacro", "n") + "<xacro:n.m/><link name=\"a${s}\"/><link name=\"b${n.s}\"/>");
    REQUIRE(include_tree::renders_link(out, "aS"));
    REQUIRE(include_tree::renders_link(out, "bS"));
}

TEST_CASE("a parent-scoped property at a namespaced file's top level lands in the including table", "[xacro][namespace]")
{
    const include_tree::tree held;
    held.write("n.xacro", include_tree::robot(property("t", "T", "parent")));
    REQUIRE(include_tree::renders_link(expand_top(held, include_as("n.xacro", "n") + "<link name=\"a${t}\"/>"), "aT"));
}

TEST_CASE("a namespace named math or xacro shadows that name for a dotted read", "[xacro][namespace]")
{
    const include_tree::tree held;
    held.write("c.xacro", include_tree::robot(property("c", "lC")));
    REQUIRE(include_tree::renders_link(expand_top(held, include_as("c.xacro", "math") + "<link name=\"${math.c}\"/>"), "lC"));
    REQUIRE(include_tree::renders_link(expand_top(held, include_as("c.xacro", "xacro") + "<link name=\"${xacro.c}\"/>"), "lC"));
}

TEST_CASE("a namespace that evaluates to a mapping refuses before anything is bound", "[xacro][namespace]")
{
    const include_tree::tree held;
    held.write("c.xacro", include_tree::robot(property("c", "lC")));
    const expansion_result out = expand_top(held, include_as("c.xacro", "${dict(a=1)}"));
    REQUIRE(refuses_with(out, meios::diagnostic_code::xacro_structural_error, "a namespace must be named by text"));
}

TEST_CASE("a namespace read as a value refuses under every policy, naming the dotted spelling", "[xacro][namespace]")
{
    const include_tree::tree held;
    held.write("c.xacro", include_tree::robot(property("c", "lC")));
    const std::string spaced = include_as("c.xacro", "n");
    for(const meios::eval_policy policy : { meios::eval_policy::fail, meios::eval_policy::warn })
        for(const std::string_view read : { "${n}", "${'a' if n else 'b'}" })
        {
            const std::string body = spaced + "<link name=\"" + std::string(read) + "\"/>";
            const expansion_result out = expand_top(held, body, policy);
            REQUIRE(refuses_with(out, meios::diagnostic_code::expression_error, "'n' names an include namespace"));
            REQUIRE(out.error().message.find("n.<name>") != std::string::npos);
            REQUIRE(error_records(held, body, policy) == 1);
        }
}

TEST_CASE("a namespace an expression stops at inside another refuses with its dotted spelling", "[xacro][namespace]")
{
    const include_tree::tree held;
    held.write("b.xacro", include_tree::robot(property("c", "lC")));
    held.write("a.xacro", include_tree::robot(include_as("b.xacro", "b")));
    const expansion_result out = expand_top(held, include_as("a.xacro", "a") + "<link name=\"${a.b}\"/>");
    REQUIRE(refuses_with(out, meios::diagnostic_code::expression_error, "'a.b' names an include namespace"));
}

TEST_CASE("a property defined after a namespace of its name reads bare as its value", "[xacro][namespace]")
{
    const include_tree::tree held;
    held.write("c.xacro", include_tree::robot(property("c", "lC")));
    const expansion_result out = expand_top(held, include_as("c.xacro", "n") + property("n", "lz") + "<link name=\"${n}\"/>");
    REQUIRE(include_tree::renders_link(out, "lz"));
}
