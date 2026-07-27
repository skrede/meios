#include "urdf_detail.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/claims.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"

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
            report(ctx, node_location(child, text, file), diagnostic_code::additional_root_element,
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
            report(ctx, node_location(child, text, file), diagnostic_code::trailing_content,
                   "trailing content after the root element", ok);
    }
}

void scan_element(pugi::xml_node element, std::string_view text, const std::filesystem::path &file,
                  parse_context &ctx, bool &ok, bool governed)
{
    std::set<std::string> seen;
    for(pugi::xml_attribute attr = element.first_attribute(); attr; attr = attr.next_attribute())
        if(!seen.insert(attr.name()).second)
            report(ctx, node_location(element, text, file), diagnostic_code::duplicate_attribute,
                   "duplicate attribute '" + std::string(attr.name()) + "'", ok);
    bool text_value = false;
    for(pugi::xml_node child : element.children())
        if(child.type() == pugi::node_pcdata && has_non_space(child.value()))
            text_value = true;
    for(pugi::xml_node child : element.children())
        if(child.type() == pugi::node_comment && text_value)
            report(ctx, node_location(child, text, file), diagnostic_code::comment_interrupting,
                   "comment interrupting element text", ok);
    if(governed && !check_vocabulary(element, text, file, ctx))
        ok = false;
    for(pugi::xml_node child : element.children())
        if(child.type() == pugi::node_element)
            scan_element(child, text, file, ctx, ok,
                         governed && vocabulary_governs(element.name(), child.name()));
}

}

void report(parse_context &ctx, const source_location &loc, diagnostic_code code,
            const std::string &message, bool &ok)
{
    if(ctx.strict == strictness::skip)
    {
        ctx.withheld |= cleared_by(code);
        return;
    }
    const level lvl = ctx.strict == strictness::fail ? level::error : level::warn;
    ctx.log.log(lvl, code, loc, message);
    if(ctx.strict == strictness::fail)
        ok = false;
}

void report_drop(parse_context &ctx, const source_location &loc, diagnostic_code code,
                 const std::string &message)
{
    bool unused = true;
    report(ctx, loc, code, message, unused);
}

void report_structural(parse_context &ctx, const source_location &loc, diagnostic_code code,
                       const std::string &message, bool &ok)
{
    ctx.log.log(level::error, code, loc, message);
    ok = false;
}

bool run_strictness(pugi::xml_node document, std::string_view text,
                    const std::filesystem::path &file, parse_context &ctx)
{
    bool ok = true;
    check_multiple_roots(document, text, file, ctx, ok);
    check_trailing_garbage(text, file, ctx, ok);
    for(pugi::xml_node child : document.children())
        if(child.type() == pugi::node_element)
            scan_element(child, text, file, ctx, ok, child.name() == std::string_view("robot"));
    return ok;
}

}
