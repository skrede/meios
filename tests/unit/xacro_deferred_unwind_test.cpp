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
#include <stdexcept>
#include <filesystem>
#include <string_view>

namespace
{

using include_tree::expansion_result;

std::string property(std::string_view name, std::string_view value)
{
    return "<xacro:property name=\"" + std::string(name) + "\" value=\"" + std::string(value) + "\"/>";
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

// `throw now` throws, `catch n` reads n and answers `caught` when that read throws, as a backend
// that turns a C++ exception into one of its own and handles it would, and anything else is read
// as a name, reported when nothing binds it.
struct throwing_backend
{
    meios::text_outcome eval_to_text(std::string_view expr, const meios::eval_scope &scope, meios::log_sink &log) const
    {
        if(expr == "throw now")
            throw std::runtime_error("abandoned");
        if(expr.starts_with("catch "))
            return caught(expr.substr(6), scope);
        const std::optional<meios::value> read = scope.lookup(expr);
        if(read && read->text())
            return meios::text_outcome{ *read->text(), meios::eval_failure_kind::none };
        log.log(meios::level::error, meios::diagnostic_code::undefined_property, meios::source_location{}, "name '" + std::string(expr) + "' is not defined");
        return meios::text_outcome{ std::nullopt, meios::eval_failure_kind::error };
    }

    static meios::text_outcome caught(std::string_view name, const meios::eval_scope &scope)
    {
        try
        {
            const std::optional<meios::value> read = scope.lookup(name);
            return meios::text_outcome{ read && read->text() ? *read->text() : std::string("absent"), meios::eval_failure_kind::none };
        }
        catch(const std::runtime_error &)
        {
            return meios::text_outcome{ std::string("caught"), meios::eval_failure_kind::none };
        }
    }
};

std::size_t links_named(const expansion_result &out, std::string_view name)
{
    const std::string spelled = "name=\"" + std::string(name) + '"';
    std::size_t count         = 0;
    for(std::size_t at = out->document.find(spelled); at != std::string::npos; at = out->document.find(spelled, at + spelled.size()))
        ++count;
    return count;
}

void heard_nothing(const error_sink_test::error_sink &heard)
{
    for(const error_sink_test::error_record &one : heard.errors)
        UNSCOPED_INFO("recorded " << one.message);
    CHECK(heard.errors.empty());
}

}

TEST_CASE("a deferred read an exception abandons leaves no step behind, so a second read is no cycle", "[xacro][deferred]")
{
    const include_tree::tree held;
    error_sink_test::error_sink heard;
    const std::string body     = property("p", "${throw now}") + "<link name=\"${catch p}\"/><link name=\"${catch p}\"/>";
    const expansion_result out = expand_with(held, body, throwing_backend{}, heard);
    heard_nothing(heard);
    REQUIRE(out.has_value());
    CHECK(links_named(out, "caught") == 2);
}

TEST_CASE("a deferred read an exception abandons leaves the call's own table current", "[xacro][deferred]")
{
    const include_tree::tree held;
    error_sink_test::error_sink heard;
    const std::string body     = property("p", "${throw now}") + macro("m", "<link name=\"${catch p}\"/>" + property("local", "one")) + "<xacro:m/><link name=\"${local}\"/>";
    const expansion_result out = expand_with(held, body, throwing_backend{}, heard);
    REQUIRE_FALSE(out.has_value());
    CHECK(out.error().message.find("name 'local' is not defined") != std::string::npos);
    REQUIRE(heard.errors.size() == 1);
    CHECK(heard.errors.front().message.find("name 'local' is not defined") != std::string::npos);
}

TEST_CASE("a deferred read an exception abandons leaves the including file's location in place", "[xacro][deferred]")
{
    const include_tree::tree held;
    held.write("w.xacro", include_tree::robot(property("p", "${throw now}")));
    error_sink_test::error_sink heard;
    const std::string body     = include_tree::include("w.xacro") + "<link name=\"${catch p}\"/>" + property("1", "x");
    const expansion_result out = expand_with(held, body, throwing_backend{}, heard);
    REQUIRE_FALSE(out.has_value());
    REQUIRE(heard.errors.size() == 1);
    CHECK(heard.errors.front().code == meios::diagnostic_code::xacro_structural_error);
    CHECK(heard.errors.front().message == "Property names must be valid python identifiers: 1");
    CHECK(heard.errors.front().loc.file.filename() == "top.xacro");
}
