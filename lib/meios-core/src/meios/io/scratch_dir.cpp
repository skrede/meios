#include "meios/io/scratch_operations.h"

#include <string>
#include <fstream>
#include <optional>
#include <filesystem>
#include <string_view>
#include <system_error>

namespace meios::detail
{

namespace
{

std::optional<scratch_root_result> refused(const operation_failure &cause)
{
    return scratch_root_result{unexpected<operation_failure>(cause)};
}

// Nothing means the name was taken and the next candidate is owed an attempt; anything else
// is the loop's answer. Narrowing is reached only from a create that succeeded, so a root
// that cannot be narrowed is removed before its refusal travels.
std::optional<scratch_root_result> attempt_root(const std::filesystem::path &parent, const scratch_operations &operations)
{
    const scratch_stem_result stem = operations.stem();
    if(!stem)
        return refused(stem.error());
    const std::filesystem::path candidate     = parent / *stem;
    const scratch_step_result created         = operations.create_exclusive(candidate);
    if(!created)
    {
        if(created.error().native.default_error_condition() == std::errc::file_exists)
            return std::nullopt;
        return refused(created.error());
    }
    const scratch_step_result narrowed = operations.narrow_to_owner(candidate);
    if(narrowed)
        return scratch_root_result{candidate};
    operations.remove_tree(candidate);
    return refused(narrowed.error());
}

}

scratch_root_result create_scratch_root(const std::filesystem::path &parent, const scratch_operations &operations)
{
    for(int attempt = 0; attempt < scratch_root_attempts; ++attempt)
        if(const std::optional<scratch_root_result> answer = attempt_root(parent, operations))
            return *answer;
    return unexpected<operation_failure>({operation_kind::create, make_error_code(std::errc::file_exists)});
}

scratch_root_result create_scratch_root(const std::filesystem::path &parent)
{
    return create_scratch_root(parent, default_scratch_operations());
}

bool write_scratch_entry(const std::filesystem::path &path, std::string_view bytes)
{
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if(ec)
        return false;
    std::ofstream out(path, std::ios::binary | std::ios::out | std::ios::trunc);
    if(!out.is_open())
        return false;
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    out.close();
    return !out.fail();
}

}
