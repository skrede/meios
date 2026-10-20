#include "expand_context.h"
#include "include_locate.h"
#include "include_routes.h"
#include "include_target.h"

#include "meios/io/resolved_asset.h"
#include "meios/io/directory_source.h"
#include "meios/io/beside_containment.h"

#include "meios/expected.h"

#include "meios/diagnostic/diagnostic_code.h"
#include "meios/diagnostic/operation_failure.h"

#include <pugixml.hpp>

#include <string>
#include <ranges>
#include <utility>
#include <optional>
#include <algorithm>
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

namespace
{

// A served document anchors its relative reads where its lookup joined it, as upstream does, while
// the canonical path stays its identity; a layer that joined no path anchors at the one it located.
std::filesystem::path served_name(const resolved_asset &hit)
{
    return hit.named().value_or(hit.path());
}

// Through a `..` the operating system may resolve a link first, so the lexical sibling record would name a file other than the one opened.
bool spelled_without_parent(const std::string &written)
{
    return std::ranges::none_of(std::filesystem::path(written), [](const std::filesystem::path &element) { return element == ".."; });
}

bool refuse_probe(expand_ctx &ctx, pugi::xml_node in, const std::string &spelling, const std::filesystem::path &looked, const operation_failure &cause)
{
    return fail(ctx, locate(ctx, in), diagnostic_code::unresolved_include, cause,
                "xacro:include could not look for \"" + spelling + "\" at \"" + looked.string() + "\": "
                    + std::string(to_string(cause.operation)) + " failed: " + cause.native.message());
}

// The layer that served the document would not serve where the link points, so it carries no served record, and a followed link's answer is final.
bool include_outside(expand_ctx &ctx, pugi::xml_node in, const std::filesystem::path &linked, const std::string &written, pugi::xml_node out)
{
    const beside_probe probe = probe_unbounded(linked);
    if(probe.cause)
        return refuse_probe(ctx, in, written, linked, *probe.cause);
    if(!probe.hit)
        return fail(ctx, in, diagnostic_code::unresolved_include,
                    "xacro:include could not resolve \"" + written + "\": followed a link to \"" + linked.string() + "\" and found nothing there");
    return enter_include(ctx, in, located_include{ *probe.hit, linked, std::nullopt }, out);
}

using followed_link = expected<std::optional<std::filesystem::path>, operation_failure>;

followed_link follow_beside(const expand_ctx &ctx, const std::filesystem::path &document, const beside_probe &probe, const std::string &written)
{
    if(!ctx.includes.follow_outside_links || probe.hit)
        return std::optional<std::filesystem::path>{};
    if(probe.uncontained)
        return beside_link(document, written);
    followed_link walked = first_beside_link(document, written);
    if(!walked || !*walked)
        return walked;
    return beside_link(document, written);
}

// The layer answers alone: a disk probe would find the file only if something earlier had made the layer write it.
bool include_served_climb(expand_ctx &ctx, pugi::xml_node in, const served_document &document, const std::string &written, pugi::xml_node out)
{
    std::optional<served_document> sibling = climbed_sibling(document, written);
    if(!sibling)
        return fail(ctx, in, diagnostic_code::xacro_structural_error,
                    "xacro:include target \"" + written + "\" climbs out of the package \"" + document.package + "\" that served the document that writes it");
    std::optional<resolved_asset> served = probe_served(ctx.sources, *sibling);
    if(!served)
        return fail(ctx, in, diagnostic_code::unresolved_include,
                    "xacro:include could not resolve \"" + written + "\": looked for it in the package \"" + sibling->package + "\" that served the document that writes it");
    const std::filesystem::path named = served_name(*served);
    return enter_include(ctx, in, located_include{ std::move(*served), named, std::move(*sibling) }, out);
}

}

bool include_from_sources(expand_ctx &ctx, pugi::xml_node in, const std::string &package, const std::string &normalized, const std::string &written, pugi::xml_node out)
{
    std::optional<served_hit> found = locate_served(ctx.sources, package, normalized, ctx.log);
    if(!found)
        return fail(ctx, in, diagnostic_code::unresolved_include, unresolved_message(package, normalized, written, !beside_anchor(ctx, package, written).empty()));
    const std::filesystem::path named = served_name(found->asset);
    return enter_include(ctx, in, located_include{ std::move(found->asset), named, served_document{ found->layer, package, normalized } }, out);
}

// A file beside the document is the one the spelling names, joined as written so a `..` resolves where the operating system resolves it, and no configured root's copy stands in
// for it: a link out of the directory it was named in, the one it lives in and every configured root refuses unless outside links are followed. [fs.op.weakly.canonical]
// decides existence through status(), which follows links, so with them unset a dangling link canonicalizes as an absent file beside the document and falls through to the
// serving layer and configured sources; followed, where a link on its way leads is the final answer.
bool include_beside(expand_ctx &ctx, pugi::xml_node in, const std::filesystem::path &anchor, const std::string &normalized, const std::string &written, pugi::xml_node out)
{
    const std::filesystem::path document = absolute_base(ctx.scope.active_document());
    const beside_probe probe             = probe_beside(document, written, ctx.package_roots);
    if(probe.cause)
        return refuse_probe(ctx, in, written, anchor / written, *probe.cause);
    const followed_link link = follow_beside(ctx, document, probe, written);
    if(!link)
        return refuse_probe(ctx, in, written, anchor / written, link.error());
    if(*link)
        return include_outside(ctx, in, **link, written, out);
    if(probe.uncontained)
        return fail(ctx, in, diagnostic_code::uncontained_asset,
                    "xacro:include target \"" + written +
                            "\" leaves every directory that admits it: the one the document that writes it was named in, the one it lives in, and each configured package root");
    std::optional<served_document> sibling;
    if(ctx.served)
        sibling = sibling_of(*ctx.served, normalized);
    if(probe.hit)
        return enter_include(ctx, in, located_include{ *probe.hit, anchor / written, probe.in_named_directory && spelled_without_parent(written) ? sibling : std::nullopt }, out);
    if(std::optional<resolved_asset> served = sibling ? probe_served(ctx.sources, *sibling) : std::nullopt)
        return enter_include(ctx, in, located_include{ *served, served_name(*served), sibling }, out);
    return include_from_sources(ctx, in, {}, normalized, written, out);
}

// Upstream opens the spelling joined as written, so `..` resolves against the real directories it
// passes through, and a miss is looked for nowhere else.
bool include_climbing(expand_ctx &ctx, pugi::xml_node in, const std::filesystem::path &anchor, const std::string &written, pugi::xml_node out)
{
    if(ctx.served && serves_bytes(ctx.sources, *ctx.served))
        return include_served_climb(ctx, in, *ctx.served, written, out);
    const std::filesystem::path joined = anchor / written;
    const beside_probe probe = probe_unbounded(joined);
    if(probe.cause)
        return refuse_probe(ctx, in, written, joined, *probe.cause);
    if(!probe.hit)
        return fail(ctx, in, diagnostic_code::unresolved_include, "xacro:include could not resolve \"" + written + "\": looked for it at \"" + joined.string() + '"');
    return enter_include(ctx, in, located_include{ *probe.hit, joined, std::nullopt }, out);
}

}
