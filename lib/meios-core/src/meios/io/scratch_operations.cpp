#include "meios/io/scratch_operations.h"

#include <random>
#include <string>
#include <cerrno>
#include <fstream>
#include <exception>
#include <filesystem>
#include <string_view>
#include <system_error>

namespace meios::detail
{
namespace
{

scratch_step_result refuse(operation_kind operation, std::error_code error)
{
    return unexpected<operation_failure>({operation, error});
}

// A stream reports failure without a code, so errno is the only native cause on offer; an
// implementation that leaves it unset would otherwise spell a failure as success.
std::error_code stream_error()
{
    return errno != 0 ? std::error_code(errno, std::generic_category()) : make_error_code(std::errc::io_error);
}

// The 128 bits defend against a local user who can watch the parent and race a name into
// place, not against chance: an unpredictable name is what makes the create a race the
// attacker cannot win.
std::string hex_stem()
{
    static const char digits[] = "0123456789abcdef";
    std::random_device device;
    std::string stem = "meios-";
    for(int i = 0; i < 16; ++i)
    {
        const unsigned value = device() & 0xffu;
        stem.push_back(digits[value >> 4]);
        stem.push_back(digits[value & 0xfu]);
    }
    return stem;
}

class native_scratch_operations final : public scratch_operations
{
public:
    // random_device's constructor and its call operator are both specified as throwing when
    // entropy cannot be obtained, and only an implementation-defined type derived from
    // exception is promised; turning that into a value is what keeps a byte-backed source's
    // construction nonthrowing.
    scratch_stem_result stem() const noexcept override
    {
        try
        {
            return hex_stem();
        }
        catch(const std::exception &)
        {
            return unexpected<operation_failure>({operation_kind::create, make_error_code(std::errc::io_error)});
        }
    }

    // create_directory is specified as-if POSIX mkdir, which fails atomically when the path
    // already exists and never follows a final symlink. It reports an occupied name in two
    // spellings — false with no error, the standard's already-a-directory recovery, and false
    // with file_exists — and both are answered here as file_exists so the policy above sees
    // one collision, never a cleared code.
    scratch_step_result create_exclusive(const std::filesystem::path &path) const noexcept override
    {
        std::error_code ec;
        if(std::filesystem::create_directory(path, ec))
            return {};
        if(!ec || ec == std::errc::file_exists)
            return refuse(operation_kind::create, make_error_code(std::errc::file_exists));
        return refuse(operation_kind::create, ec);
    }

    scratch_step_result narrow_to_owner(const std::filesystem::path &path) const noexcept override
    {
        std::error_code ec;
        std::filesystem::permissions(path, std::filesystem::perms::owner_all,
                                     std::filesystem::perm_options::replace, ec);
        return ec ? refuse(operation_kind::permissions, ec) : scratch_step_result{};
    }

    // Removal answers nothing because its own failure has no reporting site: the failure a
    // caller is told about is the one that made the root unfit to serve from. This verb and
    // write_bytes below are declared noexcept while calling operations the standard permits to
    // throw when an allocation fails — remove_all's recursive traversal and an ofstream's buffer
    // — so each turns that throw into a value rather than letting it terminate the process.
    void remove_tree(const std::filesystem::path &path) const noexcept override
    {
        try
        {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }
        catch(const std::exception &)
        {
        }
    }

    // The close is checked rather than assumed: a buffered write reaches the file there, so a
    // full disk surfaces on close and nowhere earlier. The two failures are spelled apart because
    // a stream that never opened and a stream that failed while writing are different facts about
    // the filesystem — an occupied path and a full disk — and telling a consumer which step failed
    // is what the seam is for.
    scratch_step_result write_bytes(const std::filesystem::path &path, std::string_view bytes) const noexcept override
    {
        try
        {
            errno = 0;
            std::ofstream out(path, std::ios::binary | std::ios::out | std::ios::trunc);
            if(!out.is_open())
                return refuse(operation_kind::open, stream_error());
            out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            out.close();
            if(out.fail())
                return refuse(operation_kind::write, stream_error());
            return {};
        }
        catch(const std::exception &)
        {
            return refuse(operation_kind::open, make_error_code(std::errc::not_enough_memory));
        }
    }
};

}

scratch_step_result scratch_operations::publish(const std::filesystem::path &from, const std::filesystem::path &to) const noexcept
{
    std::error_code ec;
    std::filesystem::rename(from, to, ec);
    return ec ? refuse(operation_kind::publish, ec) : scratch_step_result{};
}

const scratch_operations &default_scratch_operations() noexcept
{
    static const native_scratch_operations operations;
    return operations;
}

}
