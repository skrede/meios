#include "meios/bundle/package_writer.h"

#include "meios/io/directory_source.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"

#include <ios>
#include <string>
#include <fstream>
#include <optional>
#include <filesystem>
#include <string_view>
#include <system_error>

namespace meios
{

package_writer::package_writer(std::filesystem::path root, log_sink &log)
    : m_root(detail::absolute_base(root)), m_log(log)
{}

bool package_writer::copy_entry(const bundle_entry &entry, bool dry_run)
{
    const std::optional<std::filesystem::path> dest =
        detail::contained_candidate(m_root, "", entry.dest_relative, m_log.get());
    if(!dest)
        return false;
    if(dry_run)
    {
        std::error_code probe;
        return std::filesystem::exists(entry.copy_source, probe);
    }
    std::error_code ec;
    std::filesystem::create_directories(dest->parent_path(), ec);
    std::filesystem::copy_file(entry.copy_source, *dest,
                               std::filesystem::copy_options::overwrite_existing, ec);
    if(ec)
    {
        m_log.get().log(level::error, "failed to copy '" + entry.copy_source.string() + '\'');
        std::error_code rm;
        std::filesystem::remove(*dest, rm);
    }
    return !ec;
}

bool package_writer::write_urdf(std::string_view urdf_name, std::string_view urdf_text, bool dry_run)
{
    const std::optional<std::filesystem::path> dest =
        detail::contained_candidate(m_root, "", urdf_name, m_log.get());
    if(!dest || dry_run)
        return dest.has_value();
    std::error_code ec;
    std::filesystem::create_directories(dest->parent_path(), ec);
    std::ofstream out(*dest, std::ios::binary | std::ios::trunc);
    out.write(urdf_text.data(), static_cast<std::streamsize>(urdf_text.size()));
    out.close();
    if(out.good() && !ec)
        return true;
    std::error_code rm;
    std::filesystem::remove(*dest, rm);
    return false;
}

emit_result package_writer::write(std::string_view urdf_name, std::string_view urdf_text,
                                  const asset_manifest &manifest, bool dry_run)
{
    emit_result result{ emit_status::ok, manifest.entries.size(), manifest.unresolved.size() };
    if(!write_urdf(urdf_name, urdf_text, dry_run))
        result.status = emit_status::io_error;
    for(const bundle_entry &entry : manifest.entries)
        if(!copy_entry(entry, dry_run))
            result.status = emit_status::io_error;
    return result;
}

}
