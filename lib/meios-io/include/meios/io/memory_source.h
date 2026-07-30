#ifndef HPP_GUARD_MEIOS_IO_MEMORY_SOURCE_H
#define HPP_GUARD_MEIOS_IO_MEMORY_SOURCE_H

#include "meios/io/scratch_dir.h"
#include "meios/io/package_source.h"
#include "meios/io/resolved_asset.h"
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

// An in-memory package source keyed by (package, relative). It owns a scratch
// directory, writes an entry into it on first locate and mirrors the requested
// relative path beneath it, so a path it hands back names a real file for exactly as
// long as the source lives. It never satisfies provides_path: it cannot answer
// without writing. A source layer has no document position, so its diagnostics carry
// an empty location for the load to anchor to the input document. It declines the
// lookup that asks where a package is — the one whose relative half is empty — because
// it writes an entry only when that entry is asked for by name, so a directory it named
// would be empty and every path composed beneath it would name nothing; the lookup goes
// on to a layer that really holds the package. An entry offered under that same empty
// relative is refused, because it cannot also name a file.
class memory_source
{
    using key = std::pair<std::string, std::string>;

public:
    explicit memory_source(log_sink &log)
            : m_log(log)
            , m_entries()
            , m_paths()
            , m_scratch()
    {
        std::error_code ec;
        const std::filesystem::path parent              = std::filesystem::temp_directory_path(ec);
        const std::optional<std::filesystem::path> root = ec ? std::nullopt : detail::create_scratch_root(parent, ec);
        if(root)
        {
            m_scratch = scratch_dir{*root};
            return;
        }
        m_log.get().log(level::error, diagnostic_code::asset_write_failed, source_location{}, "could not create a scratch directory for a byte-backed source: " + ec.message());
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
            m_log.get().log(level::error, diagnostic_code::malformed_asset_uri, source_location{},
                            "refused an entry for package \"" + package +
                                    "\" offered under an empty relative path, which names the "
                                    "package directory rather than a file");
            return *this;
        }
        m_entries.insert_or_assign(key{std::move(package), std::move(relative)}, std::move(bytes));
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
        const auto cached = m_paths.find(wanted);
        if(cached != m_paths.end())
            return resolved_asset{cached->second, m_scratch.path(), request_path(package, relative)};
        const auto entry = m_entries.find(wanted);
        if(entry == m_entries.end())
            return std::nullopt;
        return write_entry(wanted, entry->second, package, relative);
    }

private:
    std::reference_wrapper<log_sink> m_log;
    std::map<key, std::string> m_entries;
    std::map<key, std::filesystem::path> m_paths;
    scratch_dir m_scratch;

    static std::filesystem::path request_path(std::string_view package, std::string_view relative)
    {
        return std::filesystem::path(package) / std::filesystem::path(relative);
    }

    std::optional<resolved_asset> refuse_without_scratch(std::string_view package, std::string_view relative)
    {
        m_log.get().log(level::error, diagnostic_code::asset_write_failed, source_location{},
                        "no scratch directory; cannot serve \"" + std::string(package) + '/' + std::string(relative) + '"');
        return std::nullopt;
    }

    std::optional<resolved_asset> write_entry(const key &wanted, const std::string &bytes, std::string_view package, std::string_view relative)
    {
        if(!m_scratch.valid())
            return refuse_without_scratch(package, relative);
        const std::optional<std::filesystem::path> target = detail::contained_candidate(m_scratch.path(), package, relative, m_log.get());
        if(!target)
            return std::nullopt;
        if(!detail::write_scratch_entry(*target, bytes))
        {
            m_log.get().log(level::error, diagnostic_code::asset_write_failed, source_location{},
                            "could not write \"" + std::string(package) + '/' + std::string(relative) + "\" into the scratch directory");
            return std::nullopt;
        }
        m_paths.insert_or_assign(wanted, *target);
        return resolved_asset{*target, m_scratch.path(), request_path(package, relative)};
    }
};

}

#endif
