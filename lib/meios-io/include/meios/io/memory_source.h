#ifndef HPP_GUARD_MEIOS_IO_MEMORY_SOURCE_H
#define HPP_GUARD_MEIOS_IO_MEMORY_SOURCE_H

#include "meios/io/scratch_dir.h"
#include "meios/io/package_source.h"
#include "meios/io/resolved_asset.h"
#include "meios/io/scratch_mirror.h"
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

namespace meios
{

// An in-memory package source. It owns a scratch directory and mirrors the requested relative
// path beneath it on first locate, so a path it hands back names a real file for as long as the
// source lives — and names the same file throughout, because an entry is identified by the
// mirrored file its two halves name rather than by the text they were spelled with, a replacement
// publishes over the path a resolution already handed out rather than beside it, and a
// publication that would reach a file another entry holds is refused before anything is written.
// Entries are immutable unless the source was built with update_behavior::replace, which is
// source-wide and fixed at construction.
// It never satisfies provides_path: it cannot answer without writing, and its diagnostics carry
// an empty location because a source layer has no document position to anchor to. It declines the
// lookup whose relative half is empty — the one asking where a package is — and refuses an offer
// naming no file beneath its own package or landing where publications stage: neither names a
// file it can serve.
class memory_source
{
    using key = detail::scratch_key;

public:
    explicit memory_source(log_sink &log, update_behavior behavior = update_behavior::reject)
            : m_behavior(behavior)
            , m_mirror()
            , m_entries()
            , m_log(log)
    {
        if(const std::optional<operation_failure> &failure = m_mirror.setup_failure())
            report_setup(*failure);
    }

    // The move specification is deduced, not declared: two std::map moves a standard library may leave throwing would terminate under an unconditional noexcept.
    memory_source(memory_source &&)                 = default;
    memory_source &operator=(memory_source &&)      = default;
    memory_source(const memory_source &)            = delete;
    memory_source &operator=(const memory_source &) = delete;

    ~memory_source() = default;

    memory_source &add(std::string package, std::string relative, std::string bytes)
    {
        if(refused_unserveable(package, relative))
            return *this;
        const key wanted                                = detail::normalized_key(package, relative);
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
        const key wanted                                       = detail::normalized_key(package, relative);
        const std::map<key, std::string>::const_iterator entry = m_entries.find(wanted);
        if(entry == m_entries.end())
            return std::nullopt;
        if(const std::optional<std::filesystem::path> served = m_mirror.materialized(wanted))
            return resolved_asset{*served, m_mirror.root(), request_path(package, relative)};
        return write_entry(wanted, entry->second, package, relative);
    }

private:
    update_behavior m_behavior;
    detail::scratch_mirror m_mirror;
    std::map<key, std::string> m_entries;
    std::reference_wrapper<log_sink> m_log;

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

    // Every branch is the same judgment: the offer names no file the source can serve. The
    // staging half is answered here rather than at the publication because the collision is made
    // by the offer, and the file that would prove it does not exist until a later resolution.
    bool refused_unserveable(const std::string &package, const std::string &relative)
    {
        if(relative.empty())
            refuse(diagnostic_code::malformed_asset_uri,
                   "refused an entry for package \"" + package +
                           "\" offered under an empty relative path, which names the "
                           "package directory rather than a file");
        else if(detail::names_scratch_staging(package, relative))
            refuse(diagnostic_code::malformed_asset_uri,
                   "refused an entry for package \"" + package + "\" offered under \"" + relative +
                           "\", which names the directory the source stages publications in rather "
                           "than a file it can serve");
        else if(detail::names_no_file_under_package(package, relative))
            refuse(diagnostic_code::malformed_asset_uri,
                   "refused an entry for package \"" + package + "\" offered under \"" + relative +
                           "\", which names no file beneath that package");
        else
            return false;
        return true;
    }

    static std::filesystem::path request_path(std::string_view package, std::string_view relative)
    {
        return std::filesystem::path(package) / std::filesystem::path(relative);
    }

    static std::string named(const key &wanted)
    {
        return '"' + wanted.first + '/' + wanted.second + '"';
    }

    bool publish_entry(const key &wanted, const std::filesystem::path &target, std::string_view bytes, const char *verb)
    {
        const expected<void, operation_failure> published = m_mirror.publish(wanted, target, bytes);
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
            refuse(diagnostic_code::duplicate_asset_entry,
                   "refused a second entry for " + named(wanted) +
                           "; a byte-backed source holds its entries immutable unless it was built to replace them, and "
                           "judges an offer by the mirrored file it names rather than by the text it was spelled with");
            return *this;
        }
        const std::optional<std::filesystem::path> target = m_mirror.materialized(wanted);
        if(target && !publish_entry(wanted, *target, bytes, "replace"))
            return *this;
        held = std::move(bytes);
        return *this;
    }

    // Placed by the key, reported by the authored halves: a candidate built from the text keeps a
    // trailing separator through canonicalization and creates a directory where the file belongs.
    std::optional<resolved_asset> write_entry(const key &wanted, const std::string &bytes, std::string_view package, std::string_view relative)
    {
        if(!m_mirror.valid())
        {
            refuse(diagnostic_code::asset_write_failed, "no scratch directory; cannot serve " + named(wanted));
            return std::nullopt;
        }
        const std::optional<std::filesystem::path> target = detail::contained_candidate(m_mirror.root(), wanted.first, wanted.second, m_log.get());
        if(!target || !publish_entry(wanted, *target, bytes, "write"))
            return std::nullopt;
        return resolved_asset{*target, m_mirror.root(), request_path(package, relative)};
    }
};

}

#endif
