#ifndef HPP_GUARD_MEIOS_UNIT_EVAL_PYTHON_FIXTURE_H
#define HPP_GUARD_MEIOS_UNIT_EVAL_PYTHON_FIXTURE_H

#include <meios/eval/python_evaluator.h>

#include <meios/xacro.h>
#include <meios/io/source_stack.h>

#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <vector>
#include <cstddef>
#include <fstream>
#include <utility>
#include <optional>
#include <filesystem>
#include <string_view>

namespace
{

std::optional<std::string> evaluate(std::string_view expr, const meios::eval_scope &scope)
{
    meios::python_evaluator evaluator;
    meios::log_sink silent;
    return evaluator.eval_to_text(expr, scope, silent);
}

struct tally
{
    int errors{ 0 };
    std::string last;

    void operator()(meios::level lvl, const std::string &message)
    {
        if(lvl != meios::level::error)
            return;
        ++errors;
        last = message;
    }

    void operator()(meios::level lvl, const meios::source_location &, const std::string &message)
    {
        (*this)(lvl, message);
    }

    void operator()(meios::level lvl, meios::diagnostic_code, const meios::source_location &,
                    const std::string &message)
    {
        (*this)(lvl, message);
    }
};

struct outcome
{
    bool ok;
    std::string document;
    int errors;
};

struct fixed_text final : meios::text_resource_loader::fetcher
{
    explicit fixed_text(std::string text) : m_text(std::move(text)) {}

    std::optional<std::string> fetch(std::string_view, const std::filesystem::path &) override
    {
        return m_text;
    }

    std::string m_text;
};

void install_yaml(meios::eval_scope &scope, std::string text)
{
    scope.install_text_loader(meios::text_resource_loader{ std::make_unique<fixed_text>(std::move(text)) });
}

outcome run(std::string_view source, meios::eval_policy policy,
            const std::shared_ptr<meios::evaluator_handle> &backend,
            std::optional<std::string> yaml = std::nullopt)
{
    tally counts;
    meios::log_sink_f sink{ std::ref(counts) };
    meios::eval_scope scope;
    if(yaml)
        install_yaml(scope, std::move(*yaml));
    meios::source_stack sources;
    meios::expansion out = meios::expand(source, scope, sources, "robot.xacro",
                                         meios::expansion_limits{}, policy, backend, sink);
    return outcome{ out.ok, std::move(out.document), counts.errors };
}

constexpr std::string_view header = "<robot xmlns:xacro=\"http://ros.org/wiki/xacro\">";

std::string span_document(std::string_view inner)
{
    return std::string(header) + "<l>" + std::string(inner) + "</l></robot>";
}

bool leaves(const outcome &result, std::string_view span)
{
    return result.document.find(span) != std::string::npos;
}

}

#endif
