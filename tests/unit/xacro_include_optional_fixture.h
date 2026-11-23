#ifndef HPP_GUARD_MEIOS_UNIT_XACRO_INCLUDE_OPTIONAL_FIXTURE_H
#define HPP_GUARD_MEIOS_UNIT_XACRO_INCLUDE_OPTIONAL_FIXTURE_H

#include "xacro_include_tree.h"

#include <meios/xacro.h>

#include <meios/io/source_stack.h>
#include <meios/io/directory_source.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/source_location.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <filesystem>

namespace
{

struct heard
{
    meios::level lvl;
    int32_t line;
    std::string message;
};

struct listener
{
    std::vector<heard> &sink;

    void operator()(meios::level lvl, const std::string &message)
    {
        sink.push_back({lvl, 0, message});
    }
    void operator()(meios::level lvl, const meios::source_location &where, const std::string &message)
    {
        sink.push_back({lvl, where.line, message});
    }
};

inline include_tree::expansion_result expand_top(const include_tree::tree &held, const std::string &body, std::vector<heard> &records)
{
    const std::filesystem::path document = held.write("doc/top.xacro", include_tree::robot(body));
    meios::log_sink_f log{listener{records}};
    meios::source_stack sources{meios::directory_source{held.root() / "root", log}};
    return include_tree::expand_file(document, sources, log);
}

inline std::string optional_include(const std::string &filename, const std::string &optional = "true")
{
    return "\n<xacro:include filename=\"" + filename + "\" optional=\"" + optional + "\"/>\n<link name=\"l\"/>";
}

inline std::size_t notes(const std::vector<heard> &records)
{
    return static_cast<std::size_t>(std::ranges::count(records, meios::level::info, &heard::lvl));
}

inline void refuses(const include_tree::expansion_result &out, const std::vector<heard> &records, meios::diagnostic_code code)
{
    REQUIRE_FALSE(out.has_value());
    REQUIRE(out.error().code == code);
    REQUIRE(notes(records) == 0);
}

}

#endif
