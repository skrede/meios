#ifndef HPP_GUARD_MEIOS_BUNDLE_PACKAGE_WRITER_H
#define HPP_GUARD_MEIOS_BUNDLE_PACKAGE_WRITER_H

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

// Writes a self-contained package rooted at a single directory: the inverse of
// bundle_source, creating dirs and copying assets into the root rather than
// resolving out of it. std::filesystem only, no new dependency. Every write
// destination is confined to the root through the shared containment guard; a
// destination that escapes is rejected with a located diagnostic and no write.
// Under dry_run it performs no create_directories/copy_file/URDF write and only
// tallies what a real write would touch.
class package_writer
{
public:
    package_writer(std::filesystem::path root, log_sink &log);

    emit_result write(std::string_view urdf_name, std::string_view urdf_text,
                      const asset_manifest &manifest, bool dry_run);

private:
    std::filesystem::path m_root;
    std::reference_wrapper<log_sink> m_log;

    bool copy_entry(const bundle_entry &entry, bool dry_run);

    bool write_urdf(std::string_view urdf_name, std::string_view urdf_text, bool dry_run);
};

// Caller-supplied bundle name is required (never derived); dry_run threads the
// same orchestration with the writer's disk mutations suppressed, so deps/assets
// reports exactly what a real bundle would touch.
struct folder_request
{
    std::filesystem::path root;
    std::string bundle_name;
    collision_options collisions;
    bool dry_run;
};

// The library-level folder-bundle entry: drives the shared emitter with a
// bundle-mode planner that wraps the rewrite/collision/closure layer, then hands
// the emitted URDF and the accumulated manifest to package_writer. Returns the
// would-be manifest (entries + unresolved list); out carries the status/counts.
asset_manifest bundle_to_folder(const tree<double> &robot, source_stack &sources,
                                scanner_registry &registry, const folder_request &request,
                                emit_result &out, log_sink &log);

asset_manifest bundle_to_folder(const model<double> &robot, source_stack &sources,
                                scanner_registry &registry, const folder_request &request,
                                emit_result &out, log_sink &log);

}

#endif
