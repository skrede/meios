#include "meios/io/scratch_operations.h"

#include <filesystem>
#include <string_view>
#include <system_error>

namespace meios::detail
{

namespace
{

// The temporary shares the target's directory because a rename between filesystems fails
// outright, and the scratch root and the system temporary directory routinely sit on
// different ones. Its name is deterministic rather than a second random draw: the containing
// directory is already unpredictably named and narrowed to its owner, so a draw buys no
// unpredictability while spending path-length budget against Windows' 260-character limit,
// which this build does not opt out of.
std::filesystem::path publication_temporary(const std::filesystem::path &target)
{
    std::filesystem::path temporary = target;
    temporary += ".meios-incoming";
    return temporary;
}

scratch_step_result stage(const std::filesystem::path &temporary, const std::filesystem::path &target, std::string_view bytes, const scratch_operations &operations)
{
    const scratch_step_result written = operations.write_bytes(temporary, bytes);
    if(!written)
        return written;
    return operations.publish(temporary, target);
}

}

// create_directories answers false with no error when the directory is already there, which is
// the normal case for every publication after the first, so only the error_code decides. The
// target is never removed or truncated ahead of the rename: that would leave the path naming
// nothing for a window, and a caller still holding the old bytes is the point.
scratch_step_result publish_scratch_entry(const std::filesystem::path &target, std::string_view bytes, const scratch_operations &operations)
{
    std::error_code ec;
    std::filesystem::create_directories(target.parent_path(), ec);
    if(ec)
        return unexpected<operation_failure>({operation_kind::create, ec});
    const std::filesystem::path temporary = publication_temporary(target);
    const scratch_step_result staged      = stage(temporary, target, bytes, operations);
    if(staged)
        return {};
    std::error_code discarded;
    std::filesystem::remove(temporary, discarded);
    return staged;
}

scratch_step_result publish_scratch_entry(const std::filesystem::path &target, std::string_view bytes)
{
    return publish_scratch_entry(target, bytes, default_scratch_operations());
}

}
