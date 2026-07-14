#include "urdf_detail.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"

#include <set>
#include <string>
#include <cctype>
#include <cstddef>
#include <algorithm>
#include <string_view>

namespace meios::detail
{

namespace
{

void report(parse_context &ctx, const source_location &loc, const std::string &message, bool &ok)
{
    const level lvl = ctx.strict == strictness::strict ? level::error : level::warn;
    ctx.log.log(lvl, loc, message);
    if(ctx.strict == strictness::strict)
        ok = false;
}

bool has_non_space(std::string_view text)
{
    return std::any_of(text.begin(), text.end(),
                       [](unsigned char c) { return std::isspace(c) == 0; });
}

void check_multiple_roots(pugi::xml_node document, std::string_view text,
                          const std::filesystem::path &file, parse_context &ctx, bool &ok)
{
    int roots = 0;
    for(pugi::xml_node child : document.children())
        if(child.type() == pugi::node_element && ++roots >= 2)
            report(ctx, node_location(child, text, file),
                   "additional root element '" + std::string(child.name()) + "'", ok);
}

// pugixml drops content after the root in document mode; a fragment reparse of
// the same bytes surfaces it as a top-level pcdata sibling to flag.
void check_trailing_garbage(std::string_view text, const std::filesystem::path &file,
                            parse_context &ctx, bool &ok)
{
    pugi::xml_document fragment;
    fragment.load_buffer(text.data(), text.size(), pugi::parse_fragment | pugi::parse_ws_pcdata);
    bool seen_root = false;
    for(pugi::xml_node child : fragment.children())
    {
        if(child.type() == pugi::node_element)
            seen_root = true;
        else if(seen_root && child.type() == pugi::node_pcdata && has_non_space(child.value()))
            report(ctx, node_location(child, text, file), "trailing content after the root element", ok);
    }
}

void scan_element(pugi::xml_node element, std::string_view text, const std::filesystem::path &file,
                  parse_context &ctx, bool &ok)
{
    std::set<std::string> seen;
    for(pugi::xml_attribute attr = element.first_attribute(); attr; attr = attr.next_attribute())
        if(!seen.insert(attr.name()).second)
            report(ctx, node_location(element, text, file),
                   "duplicate attribute '" + std::string(attr.name()) + "'", ok);
    bool text_value = false;
    for(pugi::xml_node child : element.children())
        if(child.type() == pugi::node_pcdata && has_non_space(child.value()))
            text_value = true;
    for(pugi::xml_node child : element.children())
        if(child.type() == pugi::node_comment && text_value)
            report(ctx, node_location(child, text, file), "comment interrupting element text", ok);
    for(pugi::xml_node child : element.children())
        if(child.type() == pugi::node_element)
            scan_element(child, text, file, ctx, ok);
}

}

int offset_to_line(std::string_view text, std::ptrdiff_t offset)
{
    const std::ptrdiff_t stop = std::min<std::ptrdiff_t>(offset, static_cast<std::ptrdiff_t>(text.size()));
    int line = 1;
    for(std::ptrdiff_t i = 0; i < stop; ++i)
        if(text[i] == '\n')
            ++line;
    return line;
}

source_location node_location(pugi::xml_node node, std::string_view text,
                              const std::filesystem::path &file)
{
    const std::ptrdiff_t offset = node.offset_debug();
    const std::ptrdiff_t stop = std::min<std::ptrdiff_t>(offset, static_cast<std::ptrdiff_t>(text.size()));
    int column = 1;
    for(std::ptrdiff_t i = 0; i < stop; ++i)
        column = text[i] == '\n' ? 1 : column + 1;
    return source_location{ file, offset_to_line(text, offset), column };
}

bool run_strictness(pugi::xml_node document, std::string_view text,
                    const std::filesystem::path &file, parse_context &ctx)
{
    bool ok = true;
    check_multiple_roots(document, text, file, ctx, ok);
    check_trailing_garbage(text, file, ctx, ok);
    for(pugi::xml_node child : document.children())
        if(child.type() == pugi::node_element)
            scan_element(child, text, file, ctx, ok);
    return ok;
}

}
