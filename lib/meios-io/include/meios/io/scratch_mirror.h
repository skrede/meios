#ifndef HPP_GUARD_MEIOS_IO_SCRATCH_MIRROR_H
#define HPP_GUARD_MEIOS_IO_SCRATCH_MIRROR_H

#include "meios/io/scratch_dir.h"

#include "meios/expected.h"

#include "meios/diagnostic/operation_failure.h"

#include <map>
#include <utility>
#include <optional>
#include <filesystem>
#include <string_view>
#include <system_error>

namespace meios::detail
{

// Owns the tree for its lifetime and remembers the path each key materialized at, so a
// replacement publishes over the path a resolution already handed out rather than beside it.
class scratch_mirror
{
public:
    scratch_mirror()
            : m_dir()
            , m_setup()
            , m_paths()
    {
        const expected<std::filesystem::path, operation_failure> root = open_root();
        if(root)
            m_dir = scratch_dir{*root};
        else
            m_setup = root.error();
    }

    scratch_mirror(scratch_mirror &&)                 = default;
    scratch_mirror &operator=(scratch_mirror &&)      = default;
    scratch_mirror(const scratch_mirror &)            = delete;
    scratch_mirror &operator=(const scratch_mirror &) = delete;

    ~scratch_mirror() = default;

    bool valid() const noexcept { return m_dir.valid(); }

    const std::filesystem::path &root() const noexcept { return m_dir.path(); }

    const std::optional<operation_failure> &setup_failure() const noexcept { return m_setup; }

    // A recorded path whose file went away outside the source is not a resolution: the entry is
    // republished from the bytes still held, at that same path, on the next lookup.
    std::optional<std::filesystem::path> materialized(const scratch_key &wanted) const
    {
        const std::map<scratch_key, std::filesystem::path>::const_iterator cached = m_paths.find(wanted);
        if(cached == m_paths.end())
            return std::nullopt;
        std::error_code ec;
        if(!std::filesystem::is_regular_file(cached->second, ec))
            return std::nullopt;
        return cached->second;
    }

    expected<void, operation_failure> publish(const scratch_key &wanted, const std::filesystem::path &target, std::string_view bytes)
    {
        if(const std::optional<operation_failure> held = held_by_other(wanted, target))
            return unexpected<operation_failure>(*held);
        const expected<void, operation_failure> published = publish_scratch_entry(m_dir.path(), target, bytes);
        if(published)
            m_paths.insert_or_assign(wanted, target);
        return published;
    }

private:
    scratch_dir m_dir;
    std::optional<operation_failure> m_setup;
    std::map<scratch_key, std::filesystem::path> m_paths;

    // Publishing n entries costs a scan each, and that quadratic cost is accepted: a byte-backed
    // source holds the handful an application names, and the linear alternative — a second map
    // keyed by the materialized path — is the second identity whose drift this refuses.
    // equivalent() reports every error as "not equivalent", and a recorded file that went away is
    // an error — the very state rematerialization exists to serve. The lexical arm answers that
    // case: candidates are canonicalized, so an aliasing target reaches the recorded spelling
    // exactly, while two entries genuinely naming different files never do. A pair that is one
    // file under two spellings a canonicalization keeps distinct, as a case-folding volume
    // produces, stays unanswerable while the recorded file is absent.
    std::optional<operation_failure> held_by_other(const scratch_key &wanted, const std::filesystem::path &target) const
    {
        for(const std::pair<const scratch_key, std::filesystem::path> &recorded : m_paths)
        {
            if(recorded.first == wanted)
                continue;
            std::error_code ec;
            if(std::filesystem::equivalent(recorded.second, target, ec) || (ec && recorded.second == target))
                return operation_failure{operation_kind::publish, std::make_error_code(std::errc::file_exists)};
        }
        return std::nullopt;
    }

    // The parent is resolved once here rather than at each resolution: this root is reported
    // verbatim beside paths that were themselves resolved, so the two are lexically related only if
    // it is too. Resolved weakly rather than strictly, so a temporary directory that survives the
    // status step still refuses at the creation step rather than at a third one.
    static expected<std::filesystem::path, operation_failure> open_root()
    {
        std::error_code ec;
        const std::filesystem::path named = std::filesystem::temp_directory_path(ec);
        if(ec)
            return unexpected<operation_failure>({operation_kind::status, ec});
        const std::filesystem::path parent = std::filesystem::weakly_canonical(named, ec);
        if(ec)
            return unexpected<operation_failure>({operation_kind::canonicalize, ec});
        return create_scratch_root(parent);
    }
};

}

#endif
