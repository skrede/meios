#ifndef HPP_GUARD_MEIOS_BUNDLE_MANIFEST_H
#define HPP_GUARD_MEIOS_BUNDLE_MANIFEST_H

#include "meios/bundle/result.h"
#include "meios/bundle/urdf_writer.h"
#include "meios/bundle/scanner_registry.h"

#include "meios/io/source_stack.h"

#include "meios/diagnostic/log_sink.h"

#include <map>
#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <unordered_set>

namespace meios
{

struct bundle_entry
{
    std::string rewritten_uri;
    std::filesystem::path copy_source;
    std::string dest_relative;
    std::string ref_package;
    std::string ref_dir;
};

struct asset_manifest
{
    std::vector<bundle_entry> entries;
    std::vector<std::string> unresolved;
};

// The hash-suffix collision toggle is a plain option field, deliberately not a
// {fail,warn,skip} policy enum: the default is a loud fail, and only the opt-in
// renames a same-name-different-root package to <pkg>-<hash8> to keep both.
struct collision_options
{
    bool hash_suffix_on_collision;
};

// Turns the emitter's accumulated reference set into the self-contained bundle
// manifest: each reference is parsed to its in-bundle layout and rewritten to a
// package://<bundle>/... URI, the extension-keyed scanner closure discovers
// transitive sub-assets under a cycle guard and resolves each newly discovered one
// against the source stack, and cross-package name collisions are handled per
// collision_options. A reference arriving with no resolved path is never resolved
// here: the resolver already decided about it. It performs no disk writes.
class manifest_builder
{
public:
    manifest_builder(std::string bundle_name, collision_options opts, source_stack &sources,
                     log_sink &log);

    void add_reference(const reference_record &ref);

    void close_over(scanner_registry &registry);

    const asset_manifest &manifest() const noexcept { return m_manifest; }

    // The original-reference -> package://<bundle>/... table the emitter's rewrite
    // hook consumes; keyed on the URDF's original filename so the same collision
    // decision that shapes the copy layout also shapes the in-URDF reference.
    const std::map<std::string, std::string> &rewrites() const noexcept { return m_rewrites; }

    emit_status status() const noexcept { return m_status; }

private:
    log_sink &m_log;
    emit_status m_status;
    collision_options m_opts;
    source_stack &m_sources;
    std::string m_bundle_name;
    asset_manifest m_manifest;
    std::unordered_set<std::string> m_seen;
    std::map<std::string, std::string> m_roots;
    std::map<std::string, std::string> m_rewrites;

    std::optional<std::filesystem::path> locate_source(const reference_record &ref);

    std::optional<std::string> resolve_child(const std::string &child);

    std::optional<std::string> pkg_dir(const std::string &pkg, const std::filesystem::path &root);

    void record(bool is_texture, const std::string &pkg, const std::string &rel,
                const std::filesystem::path &source, const std::string &original);

    void scan_entry(scanner_registry &registry, bundle_entry entry);
};

}

#endif
