#ifndef HPP_GUARD_MEIOS_IO_MEMORY_SOURCE_H
#define HPP_GUARD_MEIOS_IO_MEMORY_SOURCE_H

#include "meios/io/scratch_dir.h"
#include "meios/io/package_source.h"
#include "meios/io/resolved_asset.h"
#include "meios/io/update_behavior.h"
#include "meios/io/directory_source.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"
#include "meios/diagnostic/source_location.h"

#include <map>
#include <string>
#include <utility>
#include <optional>
#include <filesystem>
#include <functional>
#include <string_view>
#include <system_error>

namespace meios
{

// An in-memory package source keyed by (package, relative). It owns a scratch directory and
// mirrors the requested relative path beneath it on first locate, so a path it hands back names a
// real file for as long as the source lives — and names the same file throughout, because a
// replacement publishes over the path a resolution already handed out rather than beside it.
// Entries are immutable unless the source was built with update_behavior::replace, which is
// source-wide and fixed at construction. It never satisfies provides_path: it cannot answer
// without writing, and its diagnostics carry an empty location because a source layer has no
// document position for the load to anchor to. It declines the lookup whose relative half is
// empty — the one asking where a package is — because it writes an entry only when that entry is
// asked for by name, so the directory it named would be empty and every path beneath it would
// name nothing; an entry offered under that empty relative is refused for the mirror reason.
class memory_source
{
    using key = std::pair<std::string, std::string>;

public:
    explicit memory_source(log_sink &log, update_behavior behavior = update_behavior::reject)
            : m_scratch()
            , m_behavior(behavior)
            , m_entries()
            , m_log(log)
            , m_paths()
    {
        std::error_code ec;
        const std::filesystem::path parent = std::filesystem::temp_directory_path(ec);
        if(ec)
        {
            report_setup({operation_kind::status, ec});
            return;
        }
        const expected<std::filesystem::path, operation_failure> root = detail::create_scratch_root(parent);
        if(!root)
        {
            report_setup(root.error());
            return;
        }
        m_scratch = scratch_dir{*root};
    }

    memory_source(memory_source &&) noexcept            = default;
    memory_source &operator=(memory_source &&) noexcept = default;
    memory_source(const memory_source &)                = delete;
    memory_source &operator=(const memory_source &)     = delete;

    ~memory_source() = default;

    memory_source &add(std::string package, std::string relative, std::string bytes)
    {
        if(relative.empty())
        {
            refuse(diagnostic_code::malformed_asset_uri,
                   "refused an entry for package \"" + package +
                           "\" offered under an empty relative path, which names the "
                           "package directory rather than a file");
            return *this;
        }
        const key wanted{std::move(package), std::move(relative)};
        const std::map<key, std::string>::iterator held = m_entries.find(wanted);
        if(held != m_entries.end())
            return update(wanted, held->second, std::move(bytes));
        m_entries.emplace(wanted, std::move(bytes));
        return *this;
    }

    static capability_descriptor capabilities()
    {
        return {source_kind::memory, false};
    }

    std::optional<resolved_asset> locate(std::string_view package, std::string_view relative)
    {
        if(relative.empty())
            return std::nullopt;
        const key wanted{std::string(package), std::string(relative)};
        const std::map<key, std::string>::const_iterator entry = m_entries.find(wanted);
        if(entry == m_entries.end())
            return std::nullopt;
        if(const std::optional<std::filesystem::path> served = materialized(wanted))
            return resolved_asset{*served, m_scratch.path(), request_path(package, relative)};
        return write_entry(wanted, entry->second, package, relative);
    }

private:
    scratch_dir m_scratch;
    update_behavior m_behavior;
    std::map<key, std::string> m_entries;
    std::reference_wrapper<log_sink> m_log;
    std::map<key, std::filesystem::path> m_paths;

    void refuse(diagnostic_code code, const std::string &message)
    {
        m_log.get().log(level::error, code, source_location{}, message);
    }

    void refuse(diagnostic_code code, const operation_failure &cause, const std::string &message)
    {
        m_log.get().log(level::error, code, source_location{}, cause, message);
    }

    void report_setup(const operation_failure &cause)
    {
        refuse(diagnostic_code::asset_write_failed, cause,
               "could not create a scratch directory for a byte-backed source: " + cause.native.message());
    }

    static std::filesystem::path request_path(std::string_view package, std::string_view relative)
    {
        return std::filesystem::path(package) / std::filesystem::path(relative);
    }

    static std::string named(const key &wanted)
    {
        return '"' + wanted.first + '/' + wanted.second + '"';
    }

    // A recorded path whose file went away outside the source is not a resolution: the entry is
    // republished from the bytes still held, at that same path, on the next lookup.
    std::optional<std::filesystem::path> materialized(const key &wanted) const
    {
        const std::map<key, std::filesystem::path>::const_iterator cached = m_paths.find(wanted);
        if(cached == m_paths.end())
            return std::nullopt;
        std::error_code ec;
        if(!std::filesystem::is_regular_file(cached->second, ec))
            return std::nullopt;
        return cached->second;
    }

    bool publish_entry(const key &wanted, const std::filesystem::path &target, std::string_view bytes, const char *verb)
    {
        const expected<void, operation_failure> published = detail::publish_scratch_entry(target, bytes);
        if(published)
            return true;
        refuse(diagnostic_code::asset_write_failed, published.error(),
               std::string("could not ") + verb + ' ' + named(wanted) + " in the scratch directory");
        return false;
    }

    // The in-memory value moves only after the filesystem publication succeeded, so a refused
    // replacement leaves a later resolution serving what was last written successfully.
    memory_source &update(const key &wanted, std::string &held, std::string bytes)
    {
        if(m_behavior == update_behavior::reject)
        {
            refuse(diagnostic_code::asset_write_failed,
                   "refused a second entry for " + named(wanted) +
                           "; a byte-backed source holds its entries immutable unless it was built to replace them");
            return *this;
        }
        const std::optional<std::filesystem::path> target = materialized(wanted);
        if(target && !publish_entry(wanted, *target, bytes, "replace"))
            return *this;
        held = std::move(bytes);
        return *this;
    }

    std::optional<resolved_asset> write_entry(const key &wanted, const std::string &bytes, std::string_view package, std::string_view relative)
    {
        if(!m_scratch.valid())
        {
            refuse(diagnostic_code::asset_write_failed, "no scratch directory; cannot serve " + named(wanted));
            return std::nullopt;
        }
        const std::optional<std::filesystem::path> target = detail::contained_candidate(m_scratch.path(), package, relative, m_log.get());
        if(!target || !publish_entry(wanted, *target, bytes, "write"))
            return std::nullopt;
        m_paths.insert_or_assign(wanted, *target);
        return resolved_asset{*target, m_scratch.path(), request_path(package, relative)};
    }
};

}

#endif
