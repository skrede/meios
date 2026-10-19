#ifndef HPP_GUARD_MEIOS_UNIT_LOAD_INCLUDE_OPTIONS_FIXTURE_H
#define HPP_GUARD_MEIOS_UNIT_LOAD_INCLUDE_OPTIONS_FIXTURE_H

#include "xacro_include_tree.h"

#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#include <meios/diagnostic/log_sink.h>

#include <string>
#include <filesystem>
#include <string_view>

namespace include_options_fixture
{

inline std::filesystem::path parent_directory_probe()
{
    return std::filesystem::path{ MEIOS_XACRO_PROBE_DIR } / "include_parent_directory.xacro";
}

inline meios::include_options climbing()
{
    meios::include_options includes;
    includes.climb_above_document = true;
    return includes;
}

inline meios::include_options following()
{
    meios::include_options includes;
    includes.follow_outside_links = true;
    return includes;
}

inline include_tree::expansion_result expand_with(const std::filesystem::path &document, meios::source_stack &sources, meios::log_sink &log, const meios::include_options &includes)
{
    meios::eval_scope scope;
    return meios::expand(include_tree::read_file(document), scope, sources, document, meios::expansion_limits{}, meios::eval_policy::fail, {}, log, includes);
}

inline include_tree::expansion_result expand_top_with(std::string_view includes_text, meios::source_stack &sources, meios::log_sink &log, const meios::include_options &includes)
{
    meios::eval_scope scope;
    const std::string text = include_tree::robot(std::string(includes_text) + "<link name=\"${leaf}\"/>");
    return meios::expand(text, scope, sources, std::filesystem::path{}, meios::expansion_limits{}, meios::eval_policy::fail, {}, log, includes);
}

}

#endif
