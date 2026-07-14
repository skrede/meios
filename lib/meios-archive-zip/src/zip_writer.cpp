#include "meios/archive/zip_writer.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"

#include <miniz.h>

#include <string>
#include <utility>
#include <optional>
#include <filesystem>
#include <string_view>
#include <system_error>

namespace meios
{

namespace
{

// The zip-slip guard: an entry whose name is absolute, carries a root name, or
// lexically escapes the archive root (a leading `..` after normalization) would
// extract outside the bundle, so it is rejected with a loud diagnostic and no
// entry. Mirrors package_writer's contained_candidate invariant with no on-disk
// root; the returned name is the forward-slash generic form miniz expects.
std::optional<std::string> contained_entry(std::string_view name, log_sink &log)
{
    const std::filesystem::path p = std::filesystem::path(name).lexically_normal();
    const bool escapes = p.empty() || p.is_absolute() || p.has_root_name() ||
                         p.begin()->string() == "..";
    if(escapes)
    {
        log.log(level::error,
                "rejecting archive entry that escapes the bundle root: '" + std::string(name) + '\'');
        return std::nullopt;
    }
    return p.generic_string();
}

bool add_text(mz_zip_archive &zip, std::string_view name, std::string_view text,
              int level, log_sink &log)
{
    const std::optional<std::string> entry = contained_entry(name, log);
    if(!entry)
        return false;
    if(mz_zip_writer_add_mem(&zip, entry->c_str(), text.data(), text.size(),
                             static_cast<mz_uint>(level)))
        return true;
    log.log(level::error, "failed to archive '" + entry.value() + '\'');
    return false;
}

bool add_source(mz_zip_archive &zip, std::string_view dest,
                const std::filesystem::path &source, int level, log_sink &log)
{
    const std::optional<std::string> entry = contained_entry(dest, log);
    if(!entry)
        return false;
    if(mz_zip_writer_add_file(&zip, entry->c_str(), source.string().c_str(), nullptr, 0,
                              static_cast<mz_uint>(level)))
        return true;
    log.log(level::error, "failed to archive '" + source.string() + "': " +
            mz_zip_get_error_string(mz_zip_get_last_error(&zip)));
    return false;
}

// dry_run threads the same containment checks and exists() probes as a real write
// but touches no archive; a missing source or an escaping name still reports false.
bool probe_entries(std::string_view urdf_name, const asset_manifest &manifest, log_sink &log)
{
    bool ok = contained_entry(urdf_name, log).has_value();
    for(const bundle_entry &entry : manifest.entries)
    {
        std::error_code probe;
        const bool here = contained_entry(entry.dest_relative, log).has_value() &&
                          std::filesystem::exists(entry.copy_source, probe);
        ok = here && ok;
    }
    return ok;
}

bool write_archive(const std::filesystem::path &path, std::string_view urdf_name,
                   std::string_view urdf_text, const asset_manifest &manifest,
                   int level, log_sink &log)
{
    mz_zip_archive zip;
    mz_zip_zero_struct(&zip);
    if(!mz_zip_writer_init_file(&zip, path.string().c_str(), 0))
    {
        log.log(level::error, "failed to open archive '" + path.string() + '\'');
        return false;
    }
    bool ok = add_text(zip, urdf_name, urdf_text, level, log);
    for(const bundle_entry &entry : manifest.entries)
        ok = add_source(zip, entry.dest_relative, entry.copy_source, level, log) && ok;
    ok = mz_zip_writer_finalize_archive(&zip) && ok;
    mz_zip_writer_end(&zip);
    if(!ok)
    {
        std::error_code rm;
        std::filesystem::remove(path, rm);
    }
    return ok;
}

}

zip_writer::zip_writer(std::filesystem::path archive_path, log_sink &log, int compression_level)
    : m_level(compression_level), m_archive_path(std::move(archive_path)), m_log(log)
{}

emit_result zip_writer::write(std::string_view urdf_name, std::string_view urdf_text,
                              const asset_manifest &manifest, bool dry_run)
{
    emit_result result{ emit_status::ok, manifest.entries.size(), manifest.unresolved.size() };
    const bool ok = dry_run
        ? probe_entries(urdf_name, manifest, m_log.get())
        : write_archive(m_archive_path, urdf_name, urdf_text, manifest, m_level, m_log.get());
    if(!ok)
        result.status = emit_status::io_error;
    return result;
}

}
