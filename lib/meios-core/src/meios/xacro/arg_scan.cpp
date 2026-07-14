#include "meios/xacro/arg_scan.h"
#include "meios/xacro/budget.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"

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
    if(name.empty() || seen(out, name))
        return;
    const pugi::xml_attribute fallback = node.attribute("default");
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

bool walk(pugi::xml_node node, std::size_t ceiling, std::size_t &work,
          std::vector<arg_declaration> &out, log_sink &log)
{
    for(pugi::xml_node child : node.children())
    {
        if(++work > ceiling)
        {
            log.log(level::error, "arg scan budget exceeded");
            return false;
        }
        if(child.type() == pugi::node_element)
        {
            visit_element(child, out);
            if(!walk(child, ceiling, work, out, log))
                return false;
        }
        else if(child.type() == pugi::node_pcdata || child.type() == pugi::node_cdata)
        {
            scan_text(child.value(), out);
        }
    }
    return true;
}

}

std::vector<arg_declaration> scan_args(std::string_view source, log_sink &log)
{
    std::vector<arg_declaration> out;
    pugi::xml_document doc;
    if(!doc.load_buffer(source.data(), source.size()))
    {
        log.log(level::error, "arg scan: document did not parse");
        return out;
    }
    std::size_t work = 0;
    walk(doc, expansion_limits{}.work, work, out, log);
    return out;
}

std::vector<arg_declaration> scan_args(const std::filesystem::path &path, log_sink &log)
{
    const std::string bytes = read_file(path);
    return scan_args(std::string_view(bytes), log);
}

}
