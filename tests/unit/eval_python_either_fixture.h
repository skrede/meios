#ifndef HPP_GUARD_MEIOS_UNIT_EVAL_PYTHON_EITHER_FIXTURE_H
#define HPP_GUARD_MEIOS_UNIT_EVAL_PYTHON_EITHER_FIXTURE_H

#include <meios/eval/python_evaluator.h>
#include <meios/eval/unrestricted_python_evaluator.h>

#include <meios/xacro.h>

#include <meios/diagnostic/diagnostic_code.h>

#include <memory>
#include <string>
#include <vector>
#include <utility>
#include <optional>
#include <filesystem>
#include <functional>
#include <string_view>

namespace either_evaluator
{

struct tally
{
    std::vector<std::string> messages;

    void operator()(meios::level lvl, const std::string &message)
    {
        if(lvl == meios::level::error)
            messages.push_back(message);
    }

    void operator()(meios::level lvl, const meios::source_location &, const std::string &message)
    {
        (*this)(lvl, message);
    }

    void operator()(meios::level lvl, meios::diagnostic_code, const meios::source_location &, const std::string &message)
    {
        (*this)(lvl, message);
    }
};

struct verdict
{
    bool refused;
    std::vector<std::string> messages;
};

template<typename E>
std::optional<std::string> evaluate(std::string_view expr, const meios::eval_scope &scope)
{
    E evaluator;
    meios::log_sink silent;
    return evaluator.eval_to_text(expr, scope, silent).text;
}

template<typename E>
verdict refuse_of(std::string_view expr, const meios::eval_scope &scope)
{
    tally counts;
    meios::log_sink_f sink{std::ref(counts)};
    E evaluator;
    const meios::text_outcome got = evaluator.eval_to_text(expr, scope, sink);
    const bool declined           = !got.text && got.failure == meios::eval_failure_kind::refused;
    return {declined, std::move(counts.messages)};
}

inline bool names(const verdict &got, std::string_view fragment)
{
    for(const std::string &message : got.messages)
    {
        if(message.find(fragment) != std::string::npos)
            return true;
    }
    return false;
}

struct fixed_text final : meios::text_resource_loader::fetcher
{
    explicit fixed_text(meios::text_resource_result text)
            : m_text(std::move(text))
    {
    }

    meios::text_resource_result fetch(std::string_view, const std::filesystem::path &) override
    {
        return m_text;
    }

    meios::text_resource_result m_text;
};

inline meios::eval_scope serving(meios::text_resource_result text)
{
    meios::eval_scope scope;
    scope.install_text_loader(meios::text_resource_loader{std::make_unique<fixed_text>(std::move(text))});
    return scope;
}

}

#endif
