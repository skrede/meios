#include "structural_detail.h"

#include "meios/io/source_stack.h"
#include "meios/io/resolved_asset.h"

#include "meios/detail/text_location.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"

#include <pugixml.hpp>

#include <array>
#include <memory>
#include <string>
#include <vector>
#include <cstddef>
#include <fstream>
#include <sstream>
#include <utility>
#include <optional>
#include <filesystem>
#include <string_view>
#include <system_error>

namespace meios::detail
{

namespace
{

struct include_target
{
    std::string package;
    std::string relative;
};

include_target split_target(std::string_view raw)
{
    if(raw.rfind("package://", 0) == 0)
    {
        std::string_view rest = raw.substr(10);
        std::size_t slash = rest.find('/');
        if(slash == std::string_view::npos)
            return { std::string(rest), {} };
        return { std::string(rest.substr(0, slash)), std::string(rest.substr(slash + 1)) };
    }
    std::size_t find = raw.find("$(find ");
    std::size_t close = find == std::string_view::npos ? find : raw.find(')', find);
    if(close == std::string_view::npos)
        return { {}, std::string(raw) };
    std::string_view after = raw.substr(close + 1);
    if(!after.empty() && after.front() == '/')
        after.remove_prefix(1);
    return { std::string(raw.substr(find + 7, close - find - 7)), std::string(after) };
}

std::string normalize_relative(std::string_view rel, bool &escaped)
{
    std::vector<std::string_view> parts;
    escaped = false;
    for(std::size_t i = 0; i <= rel.size();)
    {
        std::size_t slash = rel.find('/', i);
        std::string_view seg = rel.substr(i, slash == std::string_view::npos ? slash : slash - i);
        if(seg == "..")
        {
            if(parts.empty())
                return (escaped = true, std::string{});
            parts.pop_back();
        }
        else if(!seg.empty() && seg != ".")
            parts.push_back(seg);
        if(slash == std::string_view::npos)
            break;
        i = slash + 1;
    }
    std::string out;
    for(std::string_view part : parts)
        out += (out.empty() ? "" : "/") + std::string(part);
    return out;
}

std::optional<std::string> read_asset(resolved_asset &&hit)
{
    if(hit.holds_path())
    {
        std::ifstream file(hit.path(), std::ios::binary);
        if(!file)
            return std::nullopt;
        std::ostringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
    std::string data;
    std::array<std::byte, 4096> chunk{};
    for(byte_reader &reader = hit.bytes();;)
    {
        std::size_t got = reader.read(chunk);
        if(got == 0)
            return data;
        data.append(reinterpret_cast<const char *>(chunk.data()), got);
    }
}

// The active document tracks the include stack, because a relative resource spec belongs
// to the file it is written in rather than to the file that began the expansion. It is
// not the stack's key: that key is a package-relative identity used for cycle detection,
// not a location. A layer holding only bytes materializes them into a scratch area it owns,
// so its location is the file it wrote and a relative spec beside it resolves.
bool descend(expand_ctx &ctx, pugi::xml_document &doc, const std::string &parked,
             const std::filesystem::path &key, std::filesystem::path located, pugi::xml_node out)
{
    ctx.include_stack.push_back(key);
    ctx.origins.push_back(emit_origin{ key, parked });
    std::filesystem::path enclosing = ctx.scope.set_active_document(std::move(located));
    bool ok = process_children(ctx, doc.first_child(), out, key);
    ctx.scope.set_active_document(std::move(enclosing));
    ctx.origins.pop_back();
    ctx.include_stack.pop_back();
    return ok;
}

bool splice(expand_ctx &ctx, pugi::xml_node in, resolved_asset &&hit,
            const std::filesystem::path &key, pugi::xml_node out)
{
    std::filesystem::path located = hit.holds_path() ? hit.path() : std::filesystem::path{};
    std::optional<std::string> bytes = read_asset(std::move(hit));
    if(!bytes)
        return fail(ctx, in, diagnostic_code::unresolved_include,
                    "xacro:include could not read \"" + key.string() + '"');
    // load_buffer copies into the document, but macro bodies defined in this include
    // keep string_views onto the source text, so it must outlive this call; park it in
    // owned_text alongside the parked document rather than in this local.
    ctx.owned_text.push_back(std::make_unique<std::string>(std::move(*bytes)));
    const std::string &parked = *ctx.owned_text.back();
    pugi::xml_document &doc = ctx.park();
    pugi::xml_parse_result parsed = doc.load_buffer(parked.data(), parked.size());
    if(!parsed)
        return fail(ctx, offset_location(parked, parsed.offset, key),
                    diagnostic_code::xacro_parse_error,
                    std::string("xacro:include parse error: ") + parsed.description());
    return descend(ctx, doc, parked, key, std::move(located), out);
}

}

bool expand_include(expand_ctx &ctx, pugi::xml_node in, pugi::xml_node out,
                    const std::filesystem::path &document)
{
    if(!ctx.charge_work(in))
        return false;
    include_target target = split_target(in.attribute("filename").value());
    bool ok = true;
    std::string relative = substitute_attr(ctx, in, target.relative, document, ok);
    if(!ok)
        return false;
    bool escaped = false;
    std::string normalized = normalize_relative(relative, escaped);
    if(escaped)
        return fail(ctx, in, diagnostic_code::xacro_structural_error,
                    "xacro:include target \"" + relative + "\" escapes the source root");
    std::filesystem::path joined = std::filesystem::path(target.package) / normalized;
    std::error_code canon_ec;
    std::filesystem::path canonical_key = std::filesystem::weakly_canonical(joined, canon_ec);
    std::filesystem::path key = canon_ec ? joined : canonical_key;
    for(const std::filesystem::path &seen : ctx.include_stack)
        if(seen == key)
            return fail(ctx, in, diagnostic_code::xacro_structural_error,
                        "xacro:include cycle detected re-entering \"" + key.string() + '"');
    std::optional<resolved_asset> hit = ctx.sources.locate(target.package, normalized, ctx.log);
    if(!hit)
        return fail(ctx, in, diagnostic_code::unresolved_include,
                    "xacro:include could not resolve \"" + target.package + '/' + normalized + '"');
    return splice(ctx, in, std::move(*hit), key, out);
}

}
