#include "bundle_detail.h"

#include "meios/bundle/manifest.h"

#include "meios/io/source_stack.h"
#include "meios/io/resolved_asset.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/source_location.h"

#include <map>
#include <string>
#include <utility>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios::detail
{

std::optional<parsed_reference> parse_reference(std::string_view original, bool is_texture,
                                                std::string_view bundle_name, log_sink &log)
{
    constexpr std::string_view prefix = "package://";
    std::string_view uri = original;
    if(!uri.starts_with(prefix))
        return parsed_reference{ std::string(bundle_name), std::string(original), is_texture };
    uri.remove_prefix(prefix.size());
    const std::size_t slash = uri.find('/');
    if(slash == std::string_view::npos)
    {
        log.log(level::error, source_location{ std::string(original), 0, 0 },
                "malformed package:// reference '" + std::string(original) + "'");
        return std::nullopt;
    }
    return parsed_reference{ std::string(uri.substr(0, slash)), std::string(uri.substr(slash + 1)),
                            is_texture };
}

std::string layout_dest(bool is_texture, std::string_view pkg_dir, std::string_view rel)
{
    const std::string_view root = is_texture ? "textures/" : "meshes/";
    return std::string(root) + std::string(pkg_dir) + '/' + std::string(rel);
}

std::string bundle_uri(std::string_view bundle_name, std::string_view dest)
{
    return "package://" + std::string(bundle_name) + '/' + std::string(dest);
}

std::filesystem::path source_root_of(const std::filesystem::path &resolved, std::string_view rel)
{
    std::filesystem::path root = resolved;
    const std::filesystem::path relative(rel);
    for(auto it = relative.begin(); it != relative.end(); ++it)
        root = root.parent_path();
    return root;
}

std::string compose_child_reference(std::string_view ref, std::string_view pkg,
                                    std::string_view rel_dir)
{
    if(ref.starts_with("package://"))
        return std::string(ref);
    std::string uri = "package://" + std::string(pkg) + '/';
    if(!rel_dir.empty())
        uri += std::string(rel_dir) + '/';
    return uri + std::string(ref);
}

}

namespace meios
{

manifest_builder::manifest_builder(std::string bundle_name, collision_options opts,
                                   source_stack &sources, log_sink &log)
    : m_log(log), m_status(emit_status::ok), m_opts(opts), m_sources(sources),
      m_bundle_name(std::move(bundle_name)), m_manifest(), m_seen(), m_roots(), m_rewrites()
{}

std::optional<std::filesystem::path> manifest_builder::locate_source(const reference_record &ref,
                                                                     const std::string &pkg,
                                                                     const std::string &rel)
{
    if(ref.resolved_path)
        return std::filesystem::path(*ref.resolved_path);
    const std::optional<resolved_asset> hit = m_sources.locate(pkg, rel, m_log);
    if(!hit)
    {
        m_log.log(level::warn, "could not resolve reference '" + ref.original + "'");
        return std::nullopt;
    }
    return hit->path();
}

std::optional<std::string> manifest_builder::pkg_dir(const std::string &pkg,
                                                     const std::filesystem::path &root)
{
    const std::string key = detail::normal_root(root);
    const std::map<std::string, std::string>::iterator it = m_roots.find(pkg);
    if(it == m_roots.end())
    {
        m_roots.emplace(pkg, key);
        return pkg;
    }
    if(it->second == key)
        return pkg;
    if(m_opts.hash_suffix_on_collision)
        return pkg + '-' + detail::collision_suffix(root);
    m_log.log(level::error, "package '" + pkg + "' claimed by two distinct source roots");
    m_status = emit_status::package_collision;
    return std::nullopt;
}

void manifest_builder::record(bool is_texture, const std::string &pkg, const std::string &rel,
                              const std::filesystem::path &source, const std::string &original)
{
    const std::optional<std::string> dir = pkg_dir(pkg, detail::source_root_of(source, rel));
    if(!dir)
        return;
    const std::string dest = detail::layout_dest(is_texture, *dir, rel);
    const std::string uri = detail::bundle_uri(m_bundle_name, dest);
    m_rewrites[original] = uri;
    if(!m_seen.insert(source.generic_string()).second)
        return;
    const std::string rel_dir = std::filesystem::path(rel).parent_path().generic_string();
    m_manifest.entries.push_back(bundle_entry{ uri, source, dest, pkg, rel_dir });
}

void manifest_builder::add_reference(const reference_record &ref)
{
    const std::optional<detail::parsed_reference> parsed =
        detail::parse_reference(ref.original, ref.is_texture, m_bundle_name, m_log);
    if(!parsed)
    {
        m_status = emit_status::malformed_reference;
        return;
    }
    const std::optional<std::filesystem::path> source = locate_source(ref, parsed->pkg, parsed->rel);
    if(!source)
    {
        m_manifest.unresolved.push_back(ref.original);
        return;
    }
    record(parsed->is_texture, parsed->pkg, parsed->rel, *source, ref.original);
}

}
