#ifndef HPP_GUARD_MEIOS_UNIT_XACRO_EVAL_FIXTURE_H
#define HPP_GUARD_MEIOS_UNIT_XACRO_EVAL_FIXTURE_H

#include <meios/xacro.h>

#include <meios/io/source_handle.h>
#include <meios/io/source_stack.h>
#include <meios/io/directory_source.h>

#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <random>
#include <string>
#include <vector>
#include <clocale>
#include <fstream>
#include <utility>
#include <iterator>
#include <optional>
#include <filesystem>
#include <functional>
#include <string_view>
#include <type_traits>

namespace
{


struct mock_evaluator
{
    meios::value eval(std::string_view, const meios::eval_scope &, meios::log_sink &) const
    {
        return meios::value{ 0ll };
    }
};

struct not_an_evaluator
{
    int eval(int) const { return 0; }
};

std::string eval_str(std::string_view expression, const meios::eval_scope &scope = {})
{
    meios::core_evaluator evaluator;
    meios::log_sink sink;
    return meios::to_python_str(evaluator.eval(expression, scope, sink));
}

struct failure
{
    bool failed{ false };
    int diagnostics{ 0 };
    std::string message{};

    void operator()(meios::level, const std::string &text)
    {
        message = text;
        ++diagnostics;
    }

    void operator()(meios::level, const meios::source_location &, const std::string &text)
    {
        message = text;
        ++diagnostics;
    }

    void operator()(meios::level, meios::diagnostic_code, const meios::source_location &,
                    const std::string &text)
    {
        message = text;
        ++diagnostics;
    }
};

failure eval_failure(std::string_view expression)
{
    failure state;
    meios::core_evaluator evaluator;
    meios::eval_scope scope;
    meios::log_sink_f sink{ std::ref(state) };
    evaluator.eval(expression, scope, sink);
    state.failed = evaluator.failed();
    return state;
}

meios::eval_failure_kind eval_kind(std::string_view expression)
{
    meios::core_evaluator evaluator;
    meios::eval_scope scope;
    meios::log_sink sink;
    evaluator.eval(expression, scope, sink);
    return evaluator.failure_kind();
}

}

#endif
