#include "include_locate.h"
#include "include_routes.h"
#include "include_target.h"
#include "structural_detail.h"

#include "meios/io/text_reader.h"
#include "meios/io/resolved_asset.h"

#include "meios/detail/text_location.h"

#include "meios/diagnostic/diagnostic_code.h"

#include <pugixml.hpp>

#include <memory>
#include <string>
#include <utility>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios::detail
{

namespace
{

// meios::detail also declares two-argument readers of both these names, so an unqualified call
// here would resolve to whichever declaration this translation unit happened to see first.
text_read_result read_include(const resolved_asset &hit)
{
    if(hit.source_root() && hit.source_relative())
        return meios::read_text_file_under(*hit.source_root(), *hit.source_relative());
    return meios::read_text_file(hit.path());
}

bool reenters(const expand_ctx &ctx, const std::filesystem::path &key)
{
    for(const std::filesystem::path &seen : ctx.include_stack)
        if(seen == key)
            return true;
    return false;
}

bool refuse_include(expand_ctx &ctx, pugi::xml_node in, const std::filesystem::path &key,
                    const text_read_failure &failure)
{
    return fail(ctx, locate(ctx, in), diagnostic_code::unresolved_include, failure.cause,
                "xacro:include could not read \"" + key.string() + "\": "
                    + read_failure_reason(failure));
}

// The active document is the path the file was named by and the key is the file it located; a document a
// source layer served also carries that layer and its request, so a relative include it writes is asked of
// that layer when nothing is beside it on disk.
bool descend(expand_ctx &ctx, pugi::xml_document &doc, const std::string &parked, const std::filesystem::path &key, located_include &found, pugi::xml_node out)
{
    ctx.include_stack.push_back(key);
    ctx.origins.push_back(emit_origin{ key, parked });
    std::filesystem::path enclosing = ctx.scope.set_active_document(std::move(found.named));
    std::optional<served_document> served = std::exchange(ctx.served, std::move(found.served));
    bool ok = process_children(ctx, doc.first_child(), out, key);
    ctx.served = std::move(served);
    ctx.scope.set_active_document(std::move(enclosing));
    ctx.origins.pop_back();
    ctx.include_stack.pop_back();
    return ok;
}

bool splice(expand_ctx &ctx, pugi::xml_node in, located_include &found, const std::filesystem::path &key, pugi::xml_node out)
{
    text_read_result text = read_include(found.asset);
    if(!text)
        return refuse_include(ctx, in, key, text.error());
    // load_buffer copies into the document, but macro bodies defined in this include
    // keep string_views onto the source text, so it must outlive this call; park it in
    // owned_text alongside the parked document rather than in this local.
    ctx.owned_text.push_back(std::make_unique<std::string>(std::move(*text)));
    const std::string &parked = *ctx.owned_text.back();
    pugi::xml_document &doc = ctx.park();
    pugi::xml_parse_result parsed = doc.load_buffer(parked.data(), parked.size());
    if(!parsed)
        return fail(ctx, offset_location(parked, parsed.offset, key),
                    diagnostic_code::xacro_parse_error,
                    std::string("xacro:include parse error: ") + parsed.description());
    return descend(ctx, doc, parked, key, found, out);
}

bool resolve_package(expand_ctx &ctx, pugi::xml_node in, const std::string &package)
{
    if(package.empty())
        return fail(ctx, in, diagnostic_code::unresolved_find, "$(find) requires a package name");
    if(!holds_package(ctx.sources, package))
        return fail(ctx, in, diagnostic_code::unresolved_find, "$(find " + package + ") did not resolve");
    return true;
}

// The reference lexes one token past the find before it evaluates the find, and evaluates the find
// before it reads the rest of the filename.
bool substitute_package(expand_ctx &ctx, pugi::xml_node in, const std::filesystem::path &document, std::string_view raw, include_target &target)
{
    if(!admit_follower(ctx, in, raw, *target.find_close + 1, document))
        return false;
    bool ok = true;
    target.package = substitute_attr(ctx, in, target.package, document, ok);
    return ok && resolve_package(ctx, in, target.package);
}

// Upstream joins a package:// filename as a relative path, and here it resolves through the sources as
// the recorded divergence states, so substituting its package text would widen that divergence.
bool substitute_target(expand_ctx &ctx, pugi::xml_node in, const std::filesystem::path &document, std::string_view raw, include_target &target)
{
    if(target.find_close && !substitute_package(ctx, in, document, raw, target))
        return false;
    bool ok = true;
    target.relative = substitute_attr(ctx, in, target.relative, document, ok);
    return ok;
}

bool refuse_cycle(expand_ctx &ctx, pugi::xml_node in, const std::filesystem::path &key)
{
    return fail(ctx, in, diagnostic_code::xacro_structural_error,
                "xacro:include cycle detected re-entering \"" + key.string() + '"');
}

}

bool enter_include(expand_ctx &ctx, pugi::xml_node in, located_include found, pugi::xml_node out)
{
    const std::filesystem::path key = located_key(found.asset.path());
    if(reenters(ctx, key))
        return refuse_cycle(ctx, in, key);
    return splice(ctx, in, found, key, out);
}

bool expand_include(expand_ctx &ctx, pugi::xml_node in, pugi::xml_node out,
                    const std::filesystem::path &document)
{
    if(!ctx.charge_work(in))
        return false;
    const std::string_view raw = in.attribute("filename").value();
    include_target target = split_target(raw);
    if(!substitute_target(ctx, in, document, raw, target))
        return false;
    const std::string &relative = target.relative;
    const std::filesystem::path anchor = beside_anchor(ctx, target.package, relative);
    bool escaped = false;
    std::string normalized = normalize_relative(relative, escaped);
    if(escaped && !anchor.empty() && ctx.includes.climb_above_document)
        return include_climbing(ctx, in, anchor, relative, out);
    if(escaped)
        return fail(ctx, in, diagnostic_code::xacro_structural_error,
                    "xacro:include target \"" + relative
                        + (anchor.empty() ? "\" escapes the source root" : "\" climbs above the directory of the document that writes it"));
    if(!anchor.empty())
        return include_beside(ctx, in, anchor, normalized, relative, out);
    return include_from_sources(ctx, in, target.package, normalized, relative, out);
}

}
