#ifndef HPP_GUARD_MEIOS_UNIT_ABUSE_PROBE_H
#define HPP_GUARD_MEIOS_UNIT_ABUSE_PROBE_H

#include "abuse_table.h"

#include <meios/eval/python_evaluator.h>

#include <meios/xacro.h>
#include <meios/io/source_stack.h>

#include <meios/diagnostic/diagnostic_code.h>

#include <memory>
#include <string>
#include <vector>
#include <utility>
#include <variant>
#include <optional>
#include <filesystem>
#include <string_view>

namespace abuse
{

struct tally
{
    std::vector<std::string> messages;

    void operator()(meios::level lvl, const std::string &message)
    {
        if(lvl == meios::level::error)
            messages.push_back(message);
    }

    void operator()(meios::level lvl, const meios::source_location &, const std::string &message) { (*this)(lvl, message); }

    void operator()(meios::level lvl, meios::diagnostic_code, const meios::source_location &, const std::string &message) { (*this)(lvl, message); }
};

struct verdict
{
    bool refused;
    std::vector<std::string> messages;
};

inline bool names(const verdict &got, std::string_view fragment)
{
    for(const std::string &message : got.messages)
    {
        if(message.find(fragment) != std::string::npos)
            return true;
    }
    return false;
}

template <typename E>
std::optional<std::string> evaluate(std::string_view expr, const meios::eval_scope &scope)
{
    E evaluator;
    meios::log_sink silent;
    return evaluator.eval_to_text(expr, scope, silent);
}

template <typename E>
verdict refuse_of(std::string_view expr, const meios::eval_scope &scope)
{
    tally counts;
    meios::log_sink_f sink{ std::ref(counts) };
    E evaluator;
    const bool declined = !evaluator.eval_to_text(expr, scope, sink)
        && evaluator.last_failure_kind() == meios::eval_failure_kind::refused;
    return { declined, std::move(counts.messages) };
}

struct only_config final : meios::text_resource_loader::fetcher
{
    std::optional<std::string> fetch(std::string_view spec, const std::filesystem::path &) override
    {
        if(spec == "config.yaml")
            return std::string("arm:\n  dof: 6\n");
        return std::nullopt;
    }
};

// Four rows fail for a reason the table does not claim unless these names are bound: the
// computed-attribute row, the two formatting rows carrying a description property, and the
// percent-formatting allow row, which without a bound operand raises an undefined name and would
// score as a refusal the table never claimed.
inline void seed(meios::eval_scope &scope)
{
    scope.set("x", std::string("arm"));
    scope.set("v", meios::value{ 1.5 });
    scope.set("name", std::string("arm"));
    scope.set("format", std::string("stl"));
}

inline meios::eval_scope seeded()
{
    meios::eval_scope scope;
    seed(scope);
    scope.install_text_loader(meios::text_resource_loader{ std::make_unique<only_config>() });
    return scope;
}

struct outcome
{
    bool ok;
    std::string document;
    std::vector<std::string> messages;
};

inline outcome expand_with_python(std::string_view source, meios::eval_policy policy)
{
    tally counts;
    meios::log_sink_f sink{ std::ref(counts) };
    meios::eval_scope scope;
    seed(scope);
    meios::source_stack sources;
    const auto backend = std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
    meios::expansion out = meios::expand(source, scope, sources, "robot.xacro",
                                         meios::expansion_limits{}, policy, backend, sink);
    return outcome{ out.ok, std::move(out.document), std::move(counts.messages) };
}

inline std::string span_document(const std::string &span)
{
    return "<robot xmlns:xacro=\"http://ros.org/wiki/xacro\"><l>" + span + "</l></robot>";
}

// The substitution layer answers a lone identifier bound to a string straight from the scope, so
// such a row never reaches the evaluator through a document and refuses only at the direct seam.
inline bool answered_by_scope(const row &r, const meios::eval_scope &scope)
{
    const std::optional<meios::binding> bound = scope.lookup(r.expr);
    return bound && std::holds_alternative<std::string>(*bound);
}

}

#endif
