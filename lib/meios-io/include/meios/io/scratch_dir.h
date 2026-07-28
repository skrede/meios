#ifndef HPP_GUARD_MEIOS_IO_SCRATCH_DIR_H
#define HPP_GUARD_MEIOS_IO_SCRATCH_DIR_H

#include <utility>
#include <optional>
#include <filesystem>
#include <string_view>
#include <system_error>

namespace meios
{

// Owns a directory tree for its lifetime and removes it on destruction. The
// error_code overload of remove_all is used so the destructor never throws, and a
// swap-based move keeps the retiring tree alive on the moved-from owner so a
// move-assignment target's prior tree is still removed exactly once. On Windows
// remove_all fails while a handle to a contained file is open and stops at the first
// error, so a consumer still reading a resolved asset when the owner dies leaks the
// tree rather than corrupting anything.
class scratch_dir
{
public:
    scratch_dir() = default;
    explicit scratch_dir(std::filesystem::path root) : m_path(std::move(root)) {}

    scratch_dir(scratch_dir &&other) noexcept { m_path.swap(other.m_path); }

    scratch_dir &operator=(scratch_dir &&other) noexcept
    {
        m_path.swap(other.m_path);
        return *this;
    }

    scratch_dir(const scratch_dir &) = delete;
    scratch_dir &operator=(const scratch_dir &) = delete;

    ~scratch_dir()
    {
        if(m_path.empty())
            return;
        std::error_code ec;
        std::filesystem::remove_all(m_path, ec);
    }

    const std::filesystem::path &path() const noexcept { return m_path; }

    bool valid() const noexcept { return !m_path.empty(); }

private:
    std::filesystem::path m_path;
};

namespace detail
{

// Answers a fresh directory narrowed to its owner, or nothing with the reason left in ec.
// A root that could not be narrowed is removed rather than served from: a directory other
// local users can read is worse than no directory at all.
std::optional<std::filesystem::path> create_scratch_root(const std::filesystem::path &parent,
                                                         std::error_code &ec);

bool write_scratch_entry(const std::filesystem::path &path, std::string_view bytes);

}

}

#endif
