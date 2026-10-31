#include "error_sink.h"
#include "xacro_include_tree.h"

#include <meios/xacro.h>

#include <meios/xacro/evaluator_handle.h>

#include <meios/io/source_stack.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>
#include <meios/diagnostic/source_location.h>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <cstddef>
#include <utility>
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

std::string macro(std::string_view name, std::string_view body)
{
    return "<xacro:macro name=\"" + std::string(name) + "\" params=\"\">" + std::string(body) + "</xacro:macro>";
}

template<typename Backend>
expansion_result expand_with(const include_tree::tree &held, std::string_view body, Backend backend, meios::log_sink &log)
{
    const std::filesystem::path document = held.write("top.xacro", include_tree::robot(body));
    meios::eval_scope scope;
    meios::source_stack sources{};
    const std::shared_ptr<meios::evaluator_handle> handle = std::make_shared<meios::evaluator_handle>(std::move(backend));
    return meios::expand(include_tree::read_file(document), scope, sources, document, meios::expansion_limits{}, meios::eval_policy::fail, handle, log);
}

// Reads `name`, `(name)` or `space.name` through the lookups that report a refused read, and reports
// a name nothing binds itself, as a custom evaluator would.
struct reporting_backend
{
    meios::text_outcome eval_to_text(std::string_view expr, const meios::eval_scope &scope, meios::log_sink &log) const
    {
        const std::string_view name            = expr.front() == '(' ? expr.substr(1, expr.size() - 2) : expr;
        meios::eval_failure_kind failure       = meios::eval_failure_kind::none;
        const std::optional<meios::value> read = read_through(scope, name, failure);
        if(failure != meios::eval_failure_kind::none)
            return meios::text_outcome{ std::nullopt, failure };
        if(read && read->text())
            return meios::text_outcome{ *read->text(), meios::eval_failure_kind::none };
        log.log(meios::level::error, meios::diagnostic_code::undefined_property, meios::source_location{}, "name '" + std::string(name) + "' is not defined");
        return meios::text_outcome{ std::nullopt, meios::eval_failure_kind::error };
    }

    static std::optional<meios::value> read_through(const meios::eval_scope &scope, std::string_view name, meios::eval_failure_kind &failure)
    {
        const std::size_t dot = name.find('.');
        if(dot == std::string_view::npos)
            return scope.lookup(name, failure);
        const std::optional<meios::scope_namespace> space = scope.namespace_of(name.substr(0, dot));
        return space ? scope.lookup(*space, name.substr(dot + 1), failure) : std::nullopt;
    }
};

// `keep n` keeps the handle of namespace n, `kept w` reads w through the kept handle, and `n.w`
// reads w through the namespace bound now.
struct keeping_backend
{
    std::shared_ptr<std::optional<meios::scope_namespace>> kept;

    meios::text_outcome eval_to_text(std::string_view expr, const meios::eval_scope &scope, meios::log_sink &) const
    {
        if(expr.starts_with("keep "))
        {
            *kept = scope.namespace_of(expr.substr(5));
            return meios::text_outcome{ "kept", meios::eval_failure_kind::none };
        }
        const bool through_kept                           = expr.starts_with("kept ");
        const std::size_t dot                             = expr.find('.');
        const std::optional<meios::scope_namespace> space = through_kept ? *kept : scope.namespace_of(expr.substr(0, dot));
        const std::string_view member                     = through_kept ? expr.substr(5) : expr.substr(dot + 1);
        const std::optional<meios::value> read            = space ? scope.lookup(*space, member) : std::nullopt;
        return meios::text_outcome{ read && read->text() ? *read->text() : std::string("absent"), meios::eval_failure_kind::none };
    }
};

void one_record_naming(const expansion_result &out, const error_sink_test::error_sink &heard, std::string_view naming)
{
    REQUIRE_FALSE(out.has_value());
    CHECK(out.error().message.find(naming) != std::string::npos);
    REQUIRE(heard.errors.size() == 1);
    CHECK(heard.errors.front().message.find(naming) != std::string::npos);
}

}

TEST_CASE("a custom evaluator passing on a refused deferred read leaves the refusal's one record", "[xacro][namespace]")
{
    const include_tree::tree held;
    error_sink_test::error_sink heard;
    const expansion_result out = expand_with(held, property("p", "${nope}") + "<link name=\"${(p)}\"/>", reporting_backend{}, heard);
    one_record_naming(out, heard, "name 'nope' is not defined");
}

TEST_CASE("a custom evaluator passing on a refused namespace member read leaves the refusal's one record", "[xacro][namespace]")
{
    const include_tree::tree held;
    held.write("w.xacro", include_tree::robot(property("w", "${nope}")));
    error_sink_test::error_sink heard;
    const expansion_result out = expand_with(held, include_as("w.xacro", "n") + "<link name=\"${n.w}\"/>", reporting_backend{}, heard);
    one_record_naming(out, heard, "name 'nope' is not defined");
}

TEST_CASE("a namespace handle kept past its call reads nothing, not the namespace a later call opens", "[xacro][namespace]")
{
    const include_tree::tree held;
    held.write("a.xacro", include_tree::robot(property("w", "A")));
    held.write("b.xacro", include_tree::robot(property("w", "B")));
    const std::string first  = macro("first", include_as("a.xacro", "n") + "<link name=\"${keep n}\"/><link name=\"${kept w}\"/>");
    const std::string second = macro("second", include_as("b.xacro", "n") + "<link name=\"${kept w}\"/><link name=\"${n.w}\"/>");
    error_sink_test::error_sink heard;
    const keeping_backend backend{ std::make_shared<std::optional<meios::scope_namespace>>() };
    const expansion_result out = expand_with(held, first + second + "<xacro:first/><xacro:second/>", backend, heard);
    REQUIRE(include_tree::renders_link(out, "A"));
    REQUIRE(include_tree::renders_link(out, "B"));
    REQUIRE(include_tree::renders_link(out, "absent"));
    CHECK(heard.errors.empty());
}
