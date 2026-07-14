#include "bundle_detail.h"

#include "meios/bundle/manifest.h"
#include "meios/bundle/scanner_registry.h"

#include "meios/io/resolved_asset.h"

#include "meios/diagnostic/log_sink.h"

#include <string>
#include <vector>
#include <cctype>
#include <cstddef>
#include <optional>
#include <filesystem>
#include <string_view>

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
        add_reference(reference_record{ child, std::nullopt, is_texture });
    }
}

void manifest_builder::close_over(scanner_registry &registry)
{
    for(std::size_t index = 0; index < m_manifest.entries.size(); ++index)
        scan_entry(registry, m_manifest.entries[index]);
}

}
