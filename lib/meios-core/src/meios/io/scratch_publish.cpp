#include "meios/io/scratch_operations.h"

#include <string>
#include <algorithm>
#include <filesystem>
#include <string_view>
#include <system_error>

namespace meios::detail
{

namespace
{

// The staging file stays inside the scratch root because a rename between filesystems fails
// outright, and the root and the system temporary directory routinely sit on different ones. It
// stays out of the mirrored package tree because a staging path inside that tree can be another
// entry's materialized path. Its name is deterministic rather than a second random draw: the
// containing directory is already unpredictably named and narrowed to its owner, so a draw buys
// no unpredictability while spending path-length budget against Windows' 260-character limit,
// which this build does not opt out of.
std::filesystem::path publication_temporary(const std::filesystem::path &root)
{
    return root / scratch_staging_dir / scratch_staging_file;
}

// create_directories answers false with no error when the directory is already there, which is
// the normal case for every publication after the first, so only the error_code decides.
scratch_step_result prepare_directories(const std::filesystem::path &root, const std::filesystem::path &target)
{
    std::error_code entry_ec;
    std::filesystem::create_directories(target.parent_path(), entry_ec);
    if(entry_ec)
        return unexpected<operation_failure>({operation_kind::create, entry_ec});
    std::error_code staging_ec;
    std::filesystem::create_directories(root / scratch_staging_dir, staging_ec);
    if(staging_ec)
        return unexpected<operation_failure>({operation_kind::create, staging_ec});
    return {};
}

bool same_ascii_folded(std::string_view left, std::string_view right)
{
    const auto folded = [](char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; };
    return std::equal(left.begin(), left.end(), right.begin(), right.end(),
                      [folded](char l, char r) { return folded(l) == folded(r); });
}

scratch_step_result stage(const std::filesystem::path &temporary, const std::filesystem::path &target, std::string_view bytes, const scratch_operations &operations)
{
    const scratch_step_result written = operations.write_bytes(temporary, bytes);
    if(!written)
        return written;
    return operations.publish(temporary, target);
}

}

// The fold is not decoration: the staging directory is a real directory on a filesystem that
// folds case, so a differently-cased first component names the same directory.
bool names_scratch_staging(std::string_view package, std::string_view relative)
{
    const std::filesystem::path mirrored = (std::filesystem::path(package) / std::filesystem::path(relative)).lexically_normal();
    if(mirrored.empty())
        return false;
    const std::string leading = mirrored.begin()->string();
    return same_ascii_folded(leading, scratch_staging_dir);
}

// A trailing separator survives normalization as an empty final component, so the naive split
// yields a pair naming a directory that does not exist while the spelling names an ordinary file;
// folding it back is what makes that spelling one key with the plain one. The non-empty condition
// is the loop's bound: an empty path is its own parent, so without it the fold never advances.
scratch_key normalized_key(std::string_view package, std::string_view relative)
{
    std::filesystem::path mirrored = (std::filesystem::path(package) / std::filesystem::path(relative)).lexically_normal();
    while(!mirrored.empty() && (mirrored.filename().empty() || mirrored.filename() == std::filesystem::path(".")))
        mirrored = mirrored.parent_path();
    return {mirrored.parent_path().generic_string(), mirrored.filename().generic_string()};
}

bool names_no_file_under_package(std::string_view, std::string_view relative)
{
    const std::filesystem::path named = std::filesystem::path(relative).lexically_normal();
    if(named.empty())
        return true;
    const std::string leading = named.begin()->string();
    return leading == "." || leading == "..";
}

// The target is never removed or truncated ahead of the rename: that would leave the path naming
// nothing for a window, and a caller still holding the old bytes is the point.
scratch_step_result publish_scratch_entry(const std::filesystem::path &root, const std::filesystem::path &target, std::string_view bytes, const scratch_operations &operations)
{
    const scratch_step_result prepared = prepare_directories(root, target);
    if(!prepared)
        return prepared;
    const std::filesystem::path temporary = publication_temporary(root);
    const scratch_step_result staged      = stage(temporary, target, bytes, operations);
    if(staged)
        return {};
    std::error_code discarded;
    std::filesystem::remove(temporary, discarded);
    return staged;
}

scratch_step_result publish_scratch_entry(const std::filesystem::path &root, const std::filesystem::path &target, std::string_view bytes)
{
    return publish_scratch_entry(root, target, bytes, default_scratch_operations());
}

}
