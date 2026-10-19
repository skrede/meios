#include "expand_context.h"
#include "include_locate.h"
#include "include_routes.h"
#include "include_target.h"

#include "meios/io/resolved_asset.h"
#include "meios/io/directory_source.h"

#include "meios/diagnostic/diagnostic_code.h"
#include "meios/diagnostic/operation_failure.h"

#include <pugixml.hpp>

#include <string>
#include <utility>
#include <optional>
#include <filesystem>

namespace meios::detail
{

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
    return enter_include(ctx, in, located_include{ std::move(found->asset), named, served_document{ found->layer, package, normalized } }, out);
}

namespace
{

bool refuse_probe(expand_ctx &ctx, pugi::xml_node in, const std::string &normalized, const operation_failure &cause)
{
    return fail(ctx, locate(ctx, in), diagnostic_code::unresolved_include, cause,
                "xacro:include could not look beside its document for \"" + normalized + "\": "
                    + std::string(to_string(cause.operation)) + " failed: " + cause.native.message());
}

// The layer that served the document would not serve where the link points, so it carries no served record.
bool include_outside(expand_ctx &ctx, pugi::xml_node in, const std::filesystem::path &linked, const std::string &normalized, const std::string &written, pugi::xml_node out)
{
    const beside_probe probe = probe_unbounded(linked);
    if(probe.cause)
        return refuse_probe(ctx, in, normalized, *probe.cause);
    if(!probe.hit)
        return include_from_sources(ctx, in, {}, normalized, written, out);
    return enter_include(ctx, in, located_include{ *probe.hit, linked, std::nullopt }, out);
}

}

// A file beside the document is the one the spelling names, so one that escapes the document's
// directory refuses rather than letting a configured root's same-named copy stand in for it.
bool include_beside(expand_ctx &ctx, pugi::xml_node in, const std::filesystem::path &anchor, const std::string &normalized, const std::string &written, pugi::xml_node out)
{
    const beside_probe probe = probe_beside(anchor, normalized);
    if(probe.cause)
        return refuse_probe(ctx, in, normalized, *probe.cause);
    if(probe.uncontained && ctx.includes.follow_outside_links)
        return include_outside(ctx, in, anchor / normalized, normalized, written, out);
    if(probe.uncontained)
        return fail(ctx, in, diagnostic_code::uncontained_asset,
                    "xacro:include target \"" + normalized + "\" leaves the directory of the document that writes it");
    std::optional<served_document> sibling;
    if(ctx.served)
        sibling = sibling_of(*ctx.served, normalized);
    if(probe.hit)
        return enter_include(ctx, in, located_include{ *probe.hit, anchor / normalized, sibling }, out);
    if(std::optional<resolved_asset> served = sibling ? probe_served(ctx.sources, *sibling) : std::nullopt)
        return enter_include(ctx, in, located_include{ *served, served->path(), sibling }, out);
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
    return enter_include(ctx, in, located_include{ *probe.hit, joined, std::nullopt }, out);
}

}
