#include "include_target.h"
#include "structural_detail.h"

#include "meios/io/text_reader.h"
#include "meios/io/source_stack.h"
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
#include <system_error>

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

// The active document is the path a file was named by, which anchors the relative specs written in it.
// The key is the file that path located, which identifies it for cycle detection whatever route reached it.
bool descend(expand_ctx &ctx, pugi::xml_document &doc, const std::string &parked, const std::filesystem::path &key, std::filesystem::path named, pugi::xml_node out)
{
    ctx.include_stack.push_back(key);
    ctx.origins.push_back(emit_origin{ key, parked });
    std::filesystem::path enclosing = ctx.scope.set_active_document(std::move(named));
    bool ok = process_children(ctx, doc.first_child(), out, key);
    ctx.scope.set_active_document(std::move(enclosing));
    ctx.origins.pop_back();
    ctx.include_stack.pop_back();
    return ok;
}

bool splice(expand_ctx &ctx, pugi::xml_node in, const resolved_asset &hit, const std::filesystem::path &key, std::filesystem::path named, pugi::xml_node out)
{
    text_read_result text = read_include(hit);
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
    return descend(ctx, doc, parked, key, std::move(named), out);
}

bool refuse_cycle(expand_ctx &ctx, pugi::xml_node in, const std::filesystem::path &key)
{
    return fail(ctx, in, diagnostic_code::xacro_structural_error,
                "xacro:include cycle detected re-entering \"" + key.string() + '"');
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
    std::optional<resolved_asset> hit = ctx.sources.locate(package, normalized, ctx.log);
    if(!hit)
        return fail(ctx, in, diagnostic_code::unresolved_include, unresolved_message(package, normalized, written, !beside_anchor(ctx, package, written).empty()));
    const std::filesystem::path key = located_key(hit->path());
    if(reenters(ctx, key))
        return refuse_cycle(ctx, in, key);
    return splice(ctx, in, *hit, key, hit->path(), out);
}

struct beside_probe
{
    std::optional<resolved_asset> hit;
    std::optional<operation_failure> cause;
    bool uncontained;
};

// Logs nothing: a miss falls through to the configured sources, and a logged rejection would
// latch as the terminal cause even when one of them then resolves the include. A hit is read
// under the anchor's real directory, since the confined reader follows no link on its way down.
beside_probe probe_beside(const std::filesystem::path &anchor, const std::string &normalized)
{
    std::error_code error;
    const std::filesystem::path root = std::filesystem::weakly_canonical(anchor, error);
    if(error)
        return { std::nullopt, operation_failure{ operation_kind::canonicalize, error }, false };
    const contained_path_result contained = try_contained_under(root, anchor / normalized);
    if(!contained)
        return { std::nullopt, contained.error(), false };
    if(!*contained)
        return { std::nullopt, std::nullopt, true };
    const std::filesystem::file_status status = std::filesystem::status(**contained, error);
    const bool absent = error == std::errc::no_such_file_or_directory || error == std::errc::not_a_directory;
    if(error && !absent)
        return { std::nullopt, operation_failure{ operation_kind::status, error }, false };
    if(absent || !std::filesystem::exists(status))
        return { std::nullopt, std::nullopt, false };
    return { resolved_asset{ **contained, root, (**contained).lexically_relative(root) }, std::nullopt, false };
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
    if(!probe.hit)
        return include_from_sources(ctx, in, {}, normalized, written, out);
    const std::filesystem::path key = located_key(probe.hit->path());
    if(reenters(ctx, key))
        return refuse_cycle(ctx, in, key);
    return splice(ctx, in, *probe.hit, key, anchor / normalized, out);
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
    const std::filesystem::path anchor = beside_anchor(ctx, target.package, relative);
    if(!anchor.empty())
        return include_beside(ctx, in, anchor, normalized, relative, out);
    return include_from_sources(ctx, in, target.package, normalized, relative, out);
}

}
