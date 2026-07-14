#include "bundle_detail.h"

#include "meios/bundle/manifest.h"
#include "meios/bundle/scanner_registry.h"

#include "meios/io/resolved_asset.h"

#include "meios/diagnostic/log_sink.h"

#include <string>
#include <vector>
#include <cstddef>
#include <optional>
#include <filesystem>

namespace meios::detail
{

std::string extension_of(const std::filesystem::path &source)
{
    std::string ext = source.extension().string();
    if(!ext.empty() && ext.front() == '.')
        ext.erase(ext.begin());
    return ext;
}

}

namespace meios
{

void manifest_builder::scan_entry(scanner_registry &registry, std::filesystem::path source)
{
    resolved_asset asset{ source };
    const std::vector<std::string> refs = registry.scan(detail::extension_of(source), asset, m_log);
    for(const std::string &ref : refs)
        add_reference(reference_record{ ref, std::nullopt, false });
}

void manifest_builder::close_over(scanner_registry &registry)
{
    for(std::size_t index = 0; index < m_manifest.entries.size(); ++index)
        scan_entry(registry, m_manifest.entries[index].copy_source);
}

}
