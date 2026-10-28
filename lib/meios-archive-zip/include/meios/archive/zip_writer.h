#ifndef HPP_GUARD_MEIOS_ARCHIVE_ZIP_WRITER_H
#define HPP_GUARD_MEIOS_ARCHIVE_ZIP_WRITER_H

#include "meios/bundle/result.h"
#include "meios/bundle/manifest.h"

#include "meios/model/tree.h"
#include "meios/model/model.h"

#include "meios/diagnostic/log_sink.h"

#include <string>
#include <functional>
#include <filesystem>
#include <string_view>

namespace meios
{

// MZ_DEFAULT_LEVEL
inline constexpr int default_zip_compression = 6;

// Writes a self-contained bundle into a single zip archive: the miniz-backed analog of package_writer,
// sharing its write() surface but emitting archive entries instead of a directory tree. miniz stays
// private to the .cpp, so no archive type leaks here. An entry name holding a backslash, beginning with a
// drive letter and colon, having a root, or leading with .. is refused on every host, stricter than
// package_writer's containment guard, since an extractor on any platform may read those shapes as leaving
// its target (zip-slip). A name normalizing to the archive root itself is refused too, as it names no file.
// A refusal logs an error naming the entry, and the remaining entries are still checked; write() then
// reports io_error and leaves no archive at the archive path. Under dry_run the same checks and exists()
// probes run, the result is the same io_error, and no archive file is created.
class zip_writer
{
public:
    zip_writer(std::filesystem::path archive_path, log_sink &log,
               int compression_level = default_zip_compression);

    emit_result write(std::string_view urdf_name, std::string_view urdf_text,
                      const asset_manifest &manifest, bool dry_run);

private:
    int m_level;
    std::filesystem::path m_archive_path;
    std::reference_wrapper<log_sink> m_log;
};

// Mirrors folder_request as a plain-field aggregate; compression_level threads the
// miniz level and dry_run suppresses the archive write while keeping the manifest
// planning intact, so deps/assets report exactly what a real bundle would touch.
struct zip_request
{
    std::filesystem::path archive_path;
    std::string bundle_name;
    collision_options collisions;
    int compression_level;
    bool dry_run;
};

// Re-drives the public bundle pipeline exactly as bundle_to_folder does, handing
// the emitted URDF and accumulated manifest to zip_writer instead of the folder
// writer. Returns the would-be manifest; out carries the status/counts.
asset_manifest bundle_to_zip(const tree<double> &robot, source_stack &sources,
                             scanner_registry &registry, const zip_request &request,
                             emit_result &out, log_sink &log);

asset_manifest bundle_to_zip(const model<double> &robot, source_stack &sources,
                             scanner_registry &registry, const zip_request &request,
                             emit_result &out, log_sink &log);

}

#endif
