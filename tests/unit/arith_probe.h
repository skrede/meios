#ifndef HPP_GUARD_MEIOS_UNIT_ARITH_PROBE_H
#define HPP_GUARD_MEIOS_UNIT_ARITH_PROBE_H

#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#include <string>
#include <vector>
#include <functional>
#include <string_view>

namespace arith
{

struct recorder
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

    void operator()(meios::level lvl, meios::diagnostic_code, const meios::source_location &,
                    const std::string &message)
    {
        (*this)(lvl, message);
    }
};

struct outcome
{
    bool failed;
    std::string rendered;
    std::vector<std::string> messages;
};

inline outcome evaluate(std::string_view expression)
{
    recorder heard;
    meios::log_sink_f sink{ std::ref(heard) };
    const meios::eval_scope scope;
    meios::core_evaluator evaluator;
    const meios::value result = evaluator.eval(expression, scope, sink);
    return outcome{ evaluator.failed(), meios::render_scalar(result).value_or(std::string()),
                    heard.messages };
}

inline std::string document(std::string_view expression)
{
    return std::string("<?xml version=\"1.0\"?>\n<robot name=\"arith\" "
                       "xmlns:xacro=\"http://www.ros.org/wiki/xacro\">\n"
                       "  <link name=\"a\"><visual><geometry><sphere radius=\"${")
         + std::string(expression)
         + "}\"/></geometry></visual></link>\n</robot>\n";
}

struct loaded
{
    bool expanded;
    std::string text;
    std::vector<std::string> messages;
};

inline loaded expand(std::string_view expression)
{
    recorder heard;
    meios::log_sink_f sink{ std::ref(heard) };
    const std::string source = document(expression);
    meios::eval_scope scope;
    meios::source_stack sources;
    const meios::expected<meios::expansion, meios::expansion_error> out =
        meios::expand(source, scope, sources, "arith.urdf.xacro", meios::expansion_limits{}, sink);
    return loaded{ out.has_value(), out ? out->document : std::string(), heard.messages };
}

}

#endif
