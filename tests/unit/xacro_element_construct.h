#ifndef HPP_GUARD_MEIOS_UNIT_XACRO_ELEMENT_CONSTRUCT_H
#define HPP_GUARD_MEIOS_UNIT_XACRO_ELEMENT_CONSTRUCT_H

#include <meios/xacro.h>

#include <meios/io/source_stack.h>
#include <meios/io/memory_source.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <utility>

namespace element_construct
{

using expansion_result = meios::expected<meios::expansion, meios::expansion_error>;

inline const std::string head = R"(<robot name="p" xmlns:xacro="http://www.ros.org/wiki/xacro">)";

struct warnings
{
    std::vector<std::string> &heard;

    void operator()(meios::level lvl, const std::string &message)
    {
        if(lvl == meios::level::warn)
            heard.push_back(message);
    }

    void operator()(meios::level lvl, const meios::source_location &, const std::string &message)
    {
        if(lvl == meios::level::warn)
            heard.push_back(message);
    }
};

inline expansion_result expand_with(const std::string &body, meios::source_stack &sources, std::vector<std::string> &heard)
{
    meios::log_sink_f log{warnings{heard}};
    meios::eval_scope scope;
    return meios::expand(head + body + "</robot>", scope, sources, "inline.xacro", meios::expansion_limits{}, log);
}

inline std::string rendered(const expansion_result &out)
{
    return out ? meios::canonical_xml(out->document) : "REFUSED " + out.error().message;
}

inline std::string render(const std::string &body, std::vector<std::string> &heard)
{
    meios::source_stack sources{};
    return rendered(expand_with(body, sources, heard));
}

inline std::string render(const std::string &body)
{
    std::vector<std::string> heard;
    return render(body, heard);
}

inline std::string render_with_include(const std::string &body, const std::string &included)
{
    std::vector<std::string> heard;
    meios::log_sink silent;
    meios::memory_source parts{silent};
    parts.add("pkg", "m.xacro", R"(<robot xmlns:xacro="http://www.ros.org/wiki/xacro">)" + included + "</robot>");
    meios::source_stack sources{std::move(parts)};
    return rendered(expand_with(body, sources, heard));
}

inline meios::expansion_error refused(const std::string &body,
                                     meios::diagnostic_code code = meios::diagnostic_code::xacro_structural_error)
{
    std::vector<std::string> heard;
    meios::source_stack sources{};
    const expansion_result out = expand_with(body, sources, heard);
    REQUIRE_FALSE(out.has_value());
    REQUIRE(out.error().code == code);
    return out.error();
}

}

#endif
