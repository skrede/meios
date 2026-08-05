#ifndef HPP_GUARD_MEIOS_UNIT_EVAL_POLICY_FIXTURE_H
#define HPP_GUARD_MEIOS_UNIT_EVAL_POLICY_FIXTURE_H

#include <meios/xacro.h>
#include <meios/io/source_stack.h>

#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <cstddef>
#include <optional>
#include <functional>
#include <type_traits>
#include <string_view>

namespace
{

struct tally
{
    int errors{ 0 };
    int warnings{ 0 };

    void bump(meios::level lvl)
    {
        if(lvl == meios::level::error)
            ++errors;
        else if(lvl == meios::level::warn)
            ++warnings;
    }

    void operator()(meios::level lvl, const std::string &) { bump(lvl); }
    void operator()(meios::level lvl, const meios::source_location &, const std::string &) { bump(lvl); }
    void operator()(meios::level lvl, meios::diagnostic_code, const meios::source_location &,
                    const std::string &)
    {
        bump(lvl);
    }
};

struct outcome
{
    bool ok;
    std::string document;
    int errors;
    int warnings;
};

outcome run(std::string_view source, meios::eval_policy policy,
            const std::shared_ptr<meios::evaluator_handle> &backend = {})
{
    tally counts;
    meios::log_sink_f sink{ std::ref(counts) };
    meios::eval_scope scope;
    meios::source_stack sources;
    meios::expansion out = meios::expand(source, scope, sources, "robot.xacro",
                                         meios::expansion_limits{}, policy, backend, sink);
    return outcome{ out.ok, std::move(out.document), counts.errors, counts.warnings };
}

bool leaves(const outcome &result, std::string_view span)
{
    return result.document.find(span) != std::string::npos;
}

constexpr std::string_view header = "<robot xmlns:xacro=\"http://ros.org/wiki/xacro\">";

std::string span_document(std::string_view inner)
{
    return std::string(header) + "<l>" + std::string(inner) + "</l></robot>";
}

std::string conditional_document(std::string_view test)
{
    return std::string(header) + "<xacro:if value=\"" + std::string(test) + "\"><l/></xacro:if></robot>";
}

// A backend that renders every expression to a fixed literal, proving the injected
// seam is consulted ahead of the core evaluator.
struct fixed_backend
{
    std::optional<std::string> eval_to_text(std::string_view, const meios::eval_scope &, meios::log_sink &)
    {
        return std::string("7");
    }

    meios::eval_failure_kind last_failure_kind() const { return meios::eval_failure_kind::none; }
};

struct declining_backend
{
    meios::eval_failure_kind reported{ meios::eval_failure_kind::unsupported };

    std::optional<std::string> eval_to_text(std::string_view, const meios::eval_scope &, meios::log_sink &)
    {
        return std::nullopt;
    }

    meios::eval_failure_kind last_failure_kind() const { return reported; }
};

}

#endif
