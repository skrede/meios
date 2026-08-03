#ifndef HPP_GUARD_MEIOS_IO_SCRATCH_DIR_H
#define HPP_GUARD_MEIOS_IO_SCRATCH_DIR_H

#include "meios/expected.h"

#include "meios/diagnostic/operation_failure.h"

#include <string>
#include <utility>
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

using scratch_key = std::pair<std::string, std::string>;

// Every publication stages inside one directory of this name directly under the scratch root, a
// sibling of the mirrored package directories rather than a child of any of them, so a staging
// path can never be an entry's own materialized path. The file's name is fixed-length, so a
// publication adds nothing to the entry's own path component.
inline constexpr std::string_view scratch_staging_dir  = ".meios-staging";
inline constexpr std::string_view scratch_staging_file = "incoming";

// Answers whether the mirrored path a pair would take has scratch_staging_dir as its leading
// component. The check is lexical and normalizing, so a relative that traverses into that
// directory is refused by the same predicate as the direct spelling, and it is asked when the
// entry is offered because an existence pre-check races an entry offered but not yet materialized.
bool names_scratch_staging(std::string_view package, std::string_view relative);

// Answers which mirrored file a pair names, so two spellings of one file answer one key. The
// halves are composed and normalized, then split again at the last component, which leaves an
// already-normal pair unchanged and folds every aliasing spelling onto it. It is meaningful only
// on a pair names_no_file_under_package answers false for.
scratch_key normalized_key(std::string_view package, std::string_view relative);

// Answers whether the relative half names no file strictly beneath its own package directory —
// empty once normalized, the package directory itself, or a path leading with a parent-directory
// component. It reads the relative alone rather than the composition, because a relative that
// climbs out of its package composes to an ordinary-looking path inside a different package's
// directory, which a composition-based test cannot see.
bool names_no_file_under_package(std::string_view package, std::string_view relative);

// Answers a fresh directory narrowed to its owner, or a refusal naming the step that failed
// and carrying that step's own native code. A root that could not be narrowed is removed
// rather than served from: a directory other local users can read is worse than no directory
// at all.
expected<std::filesystem::path, operation_failure> create_scratch_root(const std::filesystem::path &parent);

// Writes the bytes to a fixed-name staging file in the staging directory beneath root and
// renames it onto the target, so a reader either sees the previous bytes or the new ones and
// never a partial file. A refusal names the step that failed, carries that step's own native
// code, and leaves the target, every sibling, and the staging file as the last successful
// publication left them.
expected<void, operation_failure> publish_scratch_entry(const std::filesystem::path &root, const std::filesystem::path &target, std::string_view bytes);

}

}

#endif
