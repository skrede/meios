#include "include_locate.h"
#include "include_target.h"
#include "structural_detail.h"

#include "meios/io/text_reader.h"
#include "meios/io/resolved_asset.h"
#include "meios/io/directory_source.h"

#include "meios/detail/text_location.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"
#include "meios/diagnostic/operation_failure.h"

#include <pugixml.hpp>

#include <memory>
#include <string>
#include <utility>
#include <optional>
#include <filesystem>

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

bool refuse_cycle(expand_ctx &ctx, pugi::xml_node in, const std::filesystem::path &key)
{
    return fail(ctx, in, diagnostic_code::xacro_structural_error,
                "xacro:include cycle detected re-entering \"" + key.string() + '"');
}

bool enter(expand_ctx &ctx, pugi::xml_node in, located_include found, pugi::xml_node out)
{
    const std::filesystem::path key = located_key(found.asset.path());
    if(reenters(ctx, key))
        return refuse_cycle(ctx, in, key);
    return splice(ctx, in, found, key, out);
}

// Upstream joins a relative filename onto the directory of the file being processed; a spelling
// that is package-qualified or absolute is not looked for there, nor one in an unnamed document.
std::filesystem::path beside_anchor(const expand_ctx &ctx, const std::string &package, const std::string &relative)
{
    if(!package.empty() || std::filesystem::path(relative).has_root_path())
        return {};
    const std::filesystem::path anchor = absolute_base(ctx.scope.active_document()).parent_path();
    return anchor.is_absolute() ? anchor : std::filesystem::path{};
}

bool include_from_sources(expand_ctx &ctx, pugi::xml_node in, const std::string &package, const std::string &normalized, const std::string &written, pugi::xml_node out)
{
    std::optional<served_hit> found = locate_served(ctx.sources, package, normalized, ctx.log);
    if(!found)
        return fail(ctx, in, diagnostic_code::unresolved_include, unresolved_message(package, normalized, written, !beside_anchor(ctx, package, written).empty()));
    const std::filesystem::path named = found->asset.path();
    return enter(ctx, in, located_include{ std::move(found->asset), named, served_document{ found->layer, package, normalized } }, out);
}

bool refuse_probe(expand_ctx &ctx, pugi::xml_node in, const std::string &normalized, const operation_failure &cause)
{
    return fail(ctx, locate(ctx, in), diagnostic_code::unresolved_include, cause,
                "xacro:include could not look beside its document for \"" + normalized + "\": "
                    + std::string(to_string(cause.operation)) + " failed: " + cause.native.message());
}

// A file beside the document is the one the spelling names, so one that escapes the document's
// directory refuses rather than letting a configured root's same-named copy stand in for it.
bool include_beside(expand_ctx &ctx, pugi::xml_node in, const std::filesystem::path &anchor, const std::string &normalized, const std::string &written, pugi::xml_node out)
{
    const beside_probe probe = probe_beside(anchor, normalized);
    if(probe.cause)
        return refuse_probe(ctx, in, normalized, *probe.cause);
    if(probe.uncontained)
        return fail(ctx, in, diagnostic_code::uncontained_asset,
                    "xacro:include target \"" + normalized + "\" leaves the directory of the document that writes it");
    std::optional<served_document> sibling;
    if(ctx.served)
        sibling = sibling_of(*ctx.served, normalized);
    if(probe.hit)
        return enter(ctx, in, located_include{ *probe.hit, anchor / normalized, sibling }, out);
    if(std::optional<resolved_asset> served = sibling ? probe_served(ctx.sources, *sibling) : std::nullopt)
        return enter(ctx, in, located_include{ *served, served->path(), sibling }, out);
    return include_from_sources(ctx, in, {}, normalized, written, out);
}

// Upstream opens the spelling joined as written, so `..` resolves against the real directories it
// passes through, and a miss is looked for nowhere else.
bool include_climbing(expand_ctx &ctx, pugi::xml_node in, const std::filesystem::path &anchor, const std::string &written, pugi::xml_node out)
{
    const std::filesystem::path joined = anchor / written;
    const beside_probe probe = probe_unbounded(joined);
    if(probe.cause)
        return refuse_probe(ctx, in, written, *probe.cause);
    if(!probe.hit)
        return fail(ctx, in, diagnostic_code::unresolved_include, "xacro:include could not resolve \"" + written + "\": looked for it at \"" + joined.string() + '"');
    return enter(ctx, in, located_include{ *probe.hit, joined, std::nullopt }, out);
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
    const std::filesystem::path anchor = beside_anchor(ctx, target.package, relative);
    if(!anchor.empty() && ctx.includes.climb_above_document && climbs(relative))
        return include_climbing(ctx, in, anchor, relative, out);
    bool escaped = false;
    std::string normalized = normalize_relative(relative, escaped);
    if(escaped)
        return fail(ctx, in, diagnostic_code::xacro_structural_error,
                    "xacro:include target \"" + relative + "\" escapes the source root");
    if(!anchor.empty())
        return include_beside(ctx, in, anchor, normalized, relative, out);
    return include_from_sources(ctx, in, target.package, normalized, relative, out);
}

}
