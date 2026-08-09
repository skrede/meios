#ifndef HPP_GUARD_MEIOS_UNIT_EVAL_PYTHON_REFUSAL_FIXTURE_H
#define HPP_GUARD_MEIOS_UNIT_EVAL_PYTHON_REFUSAL_FIXTURE_H

#include "meios/urdf/yaml_resource.h"

#include <meios/eval/python_evaluator.h>

#include <meios/xacro.h>
#include <meios/io/source_stack.h>

#include <meios/diagnostic/diagnostic_code.h>
#include <meios/diagnostic/capturing_log_sink.h>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <vector>
#include <cstddef>
#include <utility>
#include <optional>
#include <algorithm>
#include <filesystem>
#include <string_view>

namespace
{

struct tally
{
    int errors{ 0 };
    int line{ 0 };
    std::string file;
    std::vector<std::string> messages;

    void record(meios::level lvl, const std::string &message)
    {
        if(lvl != meios::level::error)
            return;
        ++errors;
        messages.push_back(message);
    }

    void operator()(meios::level lvl, const std::string &message)
    {
        record(lvl, message);
    }

    void operator()(meios::level lvl, const meios::source_location &where,
                    const std::string &message)
    {
        record(lvl, message);
        anchor(lvl, where);
    }

    void operator()(meios::level lvl, meios::diagnostic_code, const meios::source_location &where,
                    const std::string &message)
    {
        record(lvl, message);
        anchor(lvl, where);
    }

    void anchor(meios::level lvl, const meios::source_location &where)
    {
        if(lvl != meios::level::error)
            return;
        file = where.file.string();
        line = where.line;
    }
};

std::optional<std::string> evaluate(std::string_view expr, const meios::eval_scope &scope)
{
    meios::python_evaluator evaluator;
    meios::log_sink silent;
    return evaluator.eval_to_text(expr, scope, silent).text;
}

struct verdict
{
    bool refused;
    std::vector<std::string> messages;
};

verdict refuse_of(std::string_view expr, const meios::eval_scope &scope)
{
    tally counts;
    meios::log_sink_f sink{ std::ref(counts) };
    meios::python_evaluator evaluator;
    const meios::text_outcome got = evaluator.eval_to_text(expr, scope, sink);
    const bool declined = !got.text && got.failure == meios::eval_failure_kind::refused;
    return { declined, std::move(counts.messages) };
}

// The acquisition layer states the terminal cause and names the resolved spec; the expression layer
// states only that the expression was refused and under which rule. A fragment may land in either,
// so it is looked for across every record rather than in the first one.
bool names(const verdict &got, std::string_view fragment)
{
    for(const std::string &message : got.messages)
    {
        if(message.find(fragment) != std::string::npos)
            return true;
    }
    return false;
}


using expansion_result = meios::expected<meios::expansion, meios::expansion_error>;

struct outcome
{
    int errors;
    int line;
    std::string file;
    expansion_result expanded;
    std::vector<std::string> messages;
};

outcome run(std::string_view source, meios::eval_policy policy,
            const std::shared_ptr<meios::evaluator_handle> &backend)
{
    tally counts;
    meios::log_sink_f sink{ std::ref(counts) };
    meios::eval_scope scope;
    meios::source_stack sources;
    expansion_result out = meios::expand(source, scope, sources, "robot.xacro",
                                         meios::expansion_limits{}, policy, backend, sink);
    return outcome{ counts.errors, counts.line, counts.file, std::move(out),
                    std::move(counts.messages) };
}

bool says(const outcome &result, std::string_view fragment)
{
    for(const std::string &message : result.messages)
    {
        if(message.find(fragment) != std::string::npos)
            return true;
    }
    return false;
}

constexpr std::string_view header = "<robot xmlns:xacro=\"http://ros.org/wiki/xacro\">";

std::string span_document(std::string_view inner)
{
    return std::string(header) + "<l>" + std::string(inner) + "</l></robot>";
}

}

#endif
