#ifndef HPP_GUARD_MEIOS_IO_SCRATCH_MIRROR_H
#define HPP_GUARD_MEIOS_IO_SCRATCH_MIRROR_H

#include "meios/io/scratch_dir.h"

#include "meios/expected.h"

#include "meios/diagnostic/operation_failure.h"

#include <map>
#include <string>
#include <utility>
#include <optional>
#include <filesystem>
#include <string_view>
#include <system_error>

namespace meios::detail
{

using scratch_key = std::pair<std::string, std::string>;

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
        const expected<void, operation_failure> published = publish_scratch_entry(m_dir.path(), target, bytes);
        if(published)
            m_paths.insert_or_assign(wanted, target);
        return published;
    }

private:
    scratch_dir m_dir;
    std::optional<operation_failure> m_setup;
    std::map<scratch_key, std::filesystem::path> m_paths;

    static expected<std::filesystem::path, operation_failure> open_root()
    {
        std::error_code ec;
        const std::filesystem::path parent = std::filesystem::temp_directory_path(ec);
        if(ec)
            return unexpected<operation_failure>({operation_kind::status, ec});
        return create_scratch_root(parent);
    }
};

}

#endif
