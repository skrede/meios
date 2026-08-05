#ifndef HPP_GUARD_MEIOS_UNIT_XACRO_ARG_FIXTURE_H
#define HPP_GUARD_MEIOS_UNIT_XACRO_ARG_FIXTURE_H

#include <meios/xacro.h>

#include <meios/io/source_stack.h>
#include <meios/io/directory_source.h>

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>
#include <functional>
#include <filesystem>
#include <string_view>

namespace
{

struct recorder
{
    int errors{ 0 };
    std::string last{};

    void note(meios::level severity, const std::string &text)
    {
        last = text;
        if(severity == meios::level::error)
            ++errors;
    }

    void operator()(meios::level severity, const std::string &text) { note(severity, text); }

    void operator()(meios::level severity, const meios::source_location &, const std::string &text)
    {
        note(severity, text);
    }

    void operator()(meios::level severity, meios::diagnostic_code, const meios::source_location &,
                    const std::string &text)
    {
        note(severity, text);
    }
};

using expansion_result = meios::expected<meios::expansion, meios::expansion_error>;

expansion_result run(std::string_view source, recorder &log)
{
    meios::source_stack sources;
    meios::eval_scope scope;
    meios::log_sink_f sink{ std::ref(log) };
    return meios::expand(source, scope, sources, "doc.xacro", meios::expansion_limits{}, sink);
}

std::string flattened(std::string_view source)
{
    recorder log;
    const expansion_result out = run(source, log);
    REQUIRE(out.has_value());
    REQUIRE(log.errors == 0);
    return meios::canonical_xml(out->document);
}

}

#endif
