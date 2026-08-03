#ifndef HPP_GUARD_MEIOS_CORE_IO_SCRATCH_OPERATIONS_H
#define HPP_GUARD_MEIOS_CORE_IO_SCRATCH_OPERATIONS_H

#include "meios/io/scratch_dir.h"

#include "meios/expected.h"

#include "meios/diagnostic/operation_failure.h"

#include <string>
#include <filesystem>

namespace meios::detail
{

using scratch_stem_result = expected<std::string, operation_failure>;
using scratch_step_result = expected<void, operation_failure>;
using scratch_root_result = expected<std::filesystem::path, operation_failure>;

// The bound exists to terminate a seeded or adversarial candidate sequence, and for nothing
// else: it is not derived from a measured collision rate and not from a measured transient
// rate. On POSIX every non-EEXIST mkdir error is permanent for the run, so no retryable
// event is left for a rate to describe.
constexpr int scratch_root_attempts = 8;

class scratch_operations
{
public:
    scratch_operations()          = default;
    virtual ~scratch_operations() = default;

    virtual scratch_stem_result stem() const noexcept                                                  = 0;
    virtual scratch_step_result create_exclusive(const std::filesystem::path &path) const noexcept     = 0;
    virtual scratch_step_result narrow_to_owner(const std::filesystem::path &path) const noexcept      = 0;
    virtual void remove_tree(const std::filesystem::path &path) const noexcept                         = 0;

protected:
    scratch_operations(const scratch_operations &)            = default;
    scratch_operations &operator=(const scratch_operations &) = default;
    scratch_operations(scratch_operations &&)                 = default;
    scratch_operations &operator=(scratch_operations &&)      = default;
};

const scratch_operations &default_scratch_operations() noexcept;
scratch_root_result create_scratch_root(const std::filesystem::path &parent, const scratch_operations &operations);

}

#endif
