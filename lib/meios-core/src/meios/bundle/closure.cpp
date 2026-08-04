#include "bundle_detail.h"

#include "meios/bundle/manifest.h"
#include "meios/bundle/scanner_registry.h"

#include "meios/io/source_stack.h"
#include "meios/io/source_lookup.h"
#include "meios/io/resolved_asset.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"
#include "meios/diagnostic/source_location.h"
#include "meios/diagnostic/operation_failure.h"

#include <string>
#include <vector>
#include <cctype>
#include <cstddef>
#include <optional>
#include <filesystem>
#include <string_view>
#include <system_error>

namespace meios::detail
{

std::string extension_of(const std::filesystem::path &source)
{
    std::string ext = source.extension().string();
    if(!ext.empty() && ext.front() == '.')
        ext.erase(ext.begin());
    return ext;
}

bool is_texture_extension(std::string ext)
{
    for(char &c : ext)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    static const std::string_view kinds[] = { "png", "jpg", "jpeg", "tga",
                                              "bmp", "gif", "dds", "tif", "tiff" };
    for(std::string_view kind : kinds)
        if(ext == kind)
            return true;
    return false;
}

}

namespace meios
{

// A discovered child is resolved here, once, the first time it is seen: the typed lookup keeps a
// native failure's terminal cause instead of collapsing it into absence, and the regular-file rule
// is the same one the asset layer applies.
std::optional<std::string> manifest_builder::resolve_child(const std::string &child)
{
    const std::optional<detail::parsed_reference> parsed = detail::parse_reference(child, false, m_bundle_name);
    if(!parsed)
        return std::nullopt;
    const source_lookup_result hit = m_sources.try_locate(parsed->pkg, parsed->rel, m_log);
    if(!hit)
    {
        m_log.log(level::error, diagnostic_code::cannot_open, source_location{}, hit.error(),
                  "cannot " + std::string(to_string(hit.error().operation)) + " reference \"" + child + "\": " + hit.error().native.message());
        return std::nullopt;
    }
    std::error_code failed;
    if(!*hit || !std::filesystem::is_regular_file((*hit)->path(), failed))
        return std::nullopt;
    return (*hit)->path().string();
}

void manifest_builder::scan_entry(scanner_registry &registry, bundle_entry entry)
{
    resolved_asset asset{ entry.copy_source };
    const std::vector<std::string> refs =
        registry.scan(detail::extension_of(entry.copy_source), asset, m_log);
    for(const std::string &ref : refs)
    {
        const std::string child =
            detail::compose_child_reference(ref, entry.ref_package, entry.ref_dir);
        const bool is_texture = detail::is_texture_extension(detail::extension_of(child));
        add_reference(reference_record{ child, resolve_child(child), is_texture });
    }
}

void manifest_builder::close_over(scanner_registry &registry)
{
    for(std::size_t index = 0; index < m_manifest.entries.size(); ++index)
        scan_entry(registry, m_manifest.entries[index]);
}

}
