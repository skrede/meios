#include "meios/xacro/arg_scan.h"
#include "meios/xacro/budget.h"

#include "meios/detail/text_location.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"

#include <pugixml.hpp>

#include <string>
#include <vector>
#include <utility>
#include <fstream>
#include <sstream>
#include <cstddef>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios
{

namespace
{

std::string read_file(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

bool seen(const std::vector<arg_declaration> &out, std::string_view name)
{
    for(const arg_declaration &arg : out)
        if(arg.name == name)
            return true;
    return false;
}

void note_reference(std::string_view inner, std::vector<arg_declaration> &out)
{
    constexpr std::string_view space = " \t\r\n";
    const std::size_t begin = inner.find_first_not_of(space);
    if(begin == std::string_view::npos)
        return;
    const std::size_t stop = inner.find_first_of(space, begin);
    const std::string_view name = inner.substr(begin, stop - begin);
    std::optional<std::string> fallback;
    const std::size_t rest = inner.find_first_not_of(space, stop);
    if(stop != std::string_view::npos && rest != std::string_view::npos)
        fallback = std::string(inner.substr(rest));
    if(!seen(out, name))
        out.push_back({ std::string(name), std::move(fallback) });
}

void scan_text(std::string_view text, std::vector<arg_declaration> &out)
{
    constexpr std::string_view tag = "$(arg ";
    std::size_t pos = text.find(tag);
    while(pos != std::string_view::npos)
    {
        const std::size_t close = text.find(')', pos);
        if(close == std::string_view::npos)
            break;
        note_reference(text.substr(pos + tag.size(), close - pos - tag.size()), out);
        pos = text.find(tag, close + 1);
    }
}

void note_declaration(pugi::xml_node node, std::vector<arg_declaration> &out)
{
    const std::string_view name = node.attribute("name").value();
    if(name.empty())
        return;
    const pugi::xml_attribute fallback = node.attribute("default");
    for(arg_declaration &arg : out)
    {
        if(arg.name != name)
            continue;
        if(!arg.default_value && fallback)
            arg.default_value = fallback.value();
        return;
    }
    out.push_back({ std::string(name),
                    fallback ? std::optional<std::string>(fallback.value()) : std::nullopt });
}

void visit_element(pugi::xml_node node, std::vector<arg_declaration> &out)
{
    if(std::string_view(node.name()) == "xacro:arg")
        note_declaration(node, out);
    for(pugi::xml_attribute attr : node.attributes())
        scan_text(attr.value(), out);
}

bool walk(pugi::xml_node node, std::string_view source, const std::filesystem::path &path,
          std::size_t ceiling, std::size_t &work, std::vector<arg_declaration> &out, log_sink &log)
{
    for(pugi::xml_node child : node.children())
    {
        if(++work > ceiling)
        {
            log.log(level::error, diagnostic_code::expansion_budget_exceeded,
                    detail::offset_location(source, child.offset_debug(), path),
                    "arg scan budget exceeded");
            return false;
        }
        if(child.type() == pugi::node_element)
        {
            visit_element(child, out);
            if(!walk(child, source, path, ceiling, work, out, log))
                return false;
        }
        else if(child.type() == pugi::node_pcdata || child.type() == pugi::node_cdata)
        {
            scan_text(child.value(), out);
        }
    }
    return true;
}

// The bare string_view overload has no document path (pre-load completion scans),
// so its emits carry an empty path with a real offset-derived line:col; the
// path-taking overload passes its real path through here.
std::vector<arg_declaration> scan_args_impl(std::string_view source,
                                            const std::filesystem::path &path, log_sink &log)
{
    std::vector<arg_declaration> out;
    pugi::xml_document doc;
    pugi::xml_parse_result parsed = doc.load_buffer(source.data(), source.size());
    if(!parsed)
    {
        log.log(level::error, diagnostic_code::xacro_parse_error,
                detail::offset_location(source, parsed.offset, path),
                "arg scan: document did not parse");
        return out;
    }
    std::size_t work = 0;
    walk(doc, source, path, expansion_limits{}.work, work, out, log);
    return out;
}

}

std::vector<arg_declaration> scan_args(std::string_view source, log_sink &log)
{
    return scan_args_impl(source, std::filesystem::path{}, log);
}

std::vector<arg_declaration> scan_args(const std::filesystem::path &path, log_sink &log)
{
    const std::string bytes = read_file(path);
    return scan_args_impl(std::string_view(bytes), path, log);
}

}
