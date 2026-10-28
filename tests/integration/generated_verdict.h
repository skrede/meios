#ifndef HPP_GUARD_MEIOS_TESTS_GENERATED_VERDICT_H
#define HPP_GUARD_MEIOS_TESTS_GENERATED_VERDICT_H

#include "generated_record.h"

#include <meios/xacro.h>

#include <meios/io/source_stack.h>
#include <meios/io/directory_source.h>

#include <meios/diagnostic/log_sink.h>

#include <string>
#include <cstddef>
#include <filesystem>
#include <string_view>

namespace generated
{

inline void replace_all(std::string &text, const std::string &from, std::string_view to)
{
    for(std::size_t at = text.find(from); !from.empty() && at != std::string::npos; at = text.find(from, at + to.size()))
        text.replace(at, from.size(), to);
}

inline std::string normalized(std::string text, const std::filesystem::path &tree)
{
    for(const std::filesystem::path &dir : {tree / "R", tree / "doc"})
    {
        const std::string_view token = dir.filename() == "R" ? "@ROOT@" : "@DOC@";
        replace_all(text, dir.string(), token);
        replace_all(text, dir.generic_string(), token);
    }
    return text;
}

// Both sides are compared as the project's own canonical text after the same path normalization, so
// attribute order and insignificant whitespace never differ while every element and attribute can.
inline std::string upstream_verdict(const generated_case &one)
{
    return one.upstream == "REFUSED" ? one.upstream : meios::canonical_xml(one.upstream);
}

inline std::string meios_verdict(const record &held, const generated_case &one, const std::filesystem::path &tree)
{
    meios::log_sink silent;
    meios::source_stack sources;
    sources.push_back(meios::source_handle(meios::directory_source(tree / "R", silent)));
    meios::eval_scope scope;
    const meios::expected<meios::expansion, meios::expansion_error> out = meios::expand(document_of(held, one), scope, sources, tree / "doc" / "case.xacro", meios::expansion_limits{},
                                                                                        meios::eval_policy::fail, nullptr, silent, meios::include_options(), {tree / "R"});
    return out ? meios::canonical_xml(normalized(out->document, tree)) : std::string("REFUSED");
}

}

#endif
