#ifndef HPP_GUARD_MEIOS_UNIT_XACRO_ATTRIBUTE_PLACE_H
#define HPP_GUARD_MEIOS_UNIT_XACRO_ATTRIBUTE_PLACE_H

#include <meios/xacro.h>

#include <meios/io/source_stack.h>
#include <meios/io/memory_source.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstdint>
#include <utility>
#include <optional>

namespace attribute_place
{

struct heard
{
    meios::level lvl;
    int32_t line;
    std::string message;
    std::optional<meios::diagnostic_code> code;
};

struct listener
{
    std::vector<heard> &sink;

    void operator()(meios::level lvl, const std::string &message)
    {
        sink.push_back({lvl, 0, message, std::nullopt});
    }

    void operator()(meios::level lvl, const meios::source_location &at, const std::string &message)
    {
        sink.push_back({lvl, at.line, message, std::nullopt});
    }

    void operator()(meios::level lvl, meios::diagnostic_code code, const meios::source_location &at, const std::string &message)
    {
        sink.push_back({lvl, at.line, message, code});
    }
};

inline const std::string head = "<robot name=\"p\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">\n";
inline const std::string placed_on_link = R"(<robot name="p"><link a="1" name="l"></link></robot>)";

inline meios::expected<meios::expansion, meios::expansion_error> expand(const std::string &body, std::vector<heard> &records,
                                                                         const std::string &included = {})
{
    meios::log_sink_f log{listener{records}};
    meios::log_sink silent;
    meios::memory_source parts{silent};
    parts.add("pkg", "i.xacro", "<robot xmlns:xacro=\"http://www.ros.org/wiki/xacro\">\n" + included + "</robot>");
    meios::source_stack sources{std::move(parts)};
    meios::eval_scope scope;
    return meios::expand(head + body + "</robot>", scope, sources, "inline.xacro", meios::expansion_limits{}, log);
}

inline std::string render(const std::string &body, std::vector<heard> &records, const std::string &included = {})
{
    const auto out = expand(body, records, included);
    return out ? meios::canonical_xml(out->document) : "REFUSED " + out.error().message;
}

inline std::string render(const std::string &body)
{
    std::vector<heard> records;
    return render(body, records);
}

inline std::vector<heard> at_level(const std::vector<heard> &records, meios::level lvl)
{
    std::vector<heard> kept;
    for(const heard &one : records)
        if(one.lvl == lvl)
            kept.push_back(one);
    return kept;
}

inline bool names(const heard &one, const std::string &text)
{
    return one.message.find(text) != std::string::npos;
}

inline void require_one_trim_warning(const std::vector<heard> &records, int32_t line, const std::string &evaluated, const std::string &used)
{
    const std::vector<heard> warnings = at_level(records, meios::level::warn);
    REQUIRE(warnings.size() == 1);
    REQUIRE(warnings[0].line == line);
    REQUIRE_FALSE(warnings[0].code.has_value());
    REQUIRE(names(warnings[0], '\'' + evaluated + '\''));
    REQUIRE(names(warnings[0], '\'' + used + '\''));
}

inline meios::expansion_error refused(const std::string &body)
{
    std::vector<heard> records;
    const auto out = expand(body, records);
    REQUIRE_FALSE(out.has_value());
    REQUIRE(out.error().code == meios::diagnostic_code::xacro_structural_error);
    return out.error();
}

inline void require_one_note(const std::string &body, int32_t line, const std::string &included = {})
{
    std::vector<heard> records;
    REQUIRE(render(body, records, included) == placed_on_link);
    const std::vector<heard> notes = at_level(records, meios::level::info);
    REQUIRE(notes.size() == 1);
    REQUIRE(notes[0].line == line);
    REQUIRE(names(notes[0], "'a'"));
    REQUIRE(names(notes[0], "<link>"));
    REQUIRE(at_level(records, meios::level::warn).empty());
}

}

#endif
