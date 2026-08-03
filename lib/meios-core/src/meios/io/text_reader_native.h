#ifndef HPP_GUARD_MEIOS_CORE_IO_TEXT_READER_NATIVE_H
#define HPP_GUARD_MEIOS_CORE_IO_TEXT_READER_NATIVE_H

#include "meios/io/text_reader.h"

#include "meios/expected.h"

#include <cerrno>
#include <cstdio>
#include <vector>
#include <utility>
#include <algorithm>
#include <filesystem>
#include <system_error>

namespace meios::detail
{

using native_file_result = expected<std::FILE *, text_read_failure>;

inline text_read_failure native_failure(text_read_failure_kind kind, operation_kind operation, std::error_code error)
{
    return {kind, {operation, error}};
}

inline std::error_code native_crt_error() noexcept
{
    return errno == 0 ? make_error_code(std::errc::io_error) : std::error_code{errno, std::generic_category()};
}

native_file_result native_open(const std::filesystem::path &path) noexcept;
native_file_result native_open_under(const std::filesystem::path &root, const std::filesystem::path &relative) noexcept;

}

#if defined(_WIN32)

    #include "meios/io/text_reader_native_windows.h"

#else

    #include <fcntl.h>
    #include <unistd.h>
    #include <sys/stat.h>

namespace meios::detail
{
namespace native
{

class fd_owner
{
public:
    explicit fd_owner(int fd) noexcept
            : m_fd(fd)
    {
    }
    fd_owner(fd_owner &&other) noexcept
            : m_fd(std::exchange(other.m_fd, -1))
    {
    }
    fd_owner &operator=(fd_owner &&)      = delete;
    fd_owner(const fd_owner &)            = delete;
    fd_owner &operator=(const fd_owner &) = delete;
    ~fd_owner()
    {
        if(m_fd >= 0)
            (void)::close(m_fd);
    }
    int get() const noexcept
    {
        return m_fd;
    }
    int release() noexcept
    {
        return std::exchange(m_fd, -1);
    }

private:
    int m_fd;
};

inline bool valid_relative(const std::filesystem::path &relative)
{
    if(relative.empty() || relative.is_absolute() || relative.has_root_path())
        return false;
    return std::ranges::none_of(relative, [](const std::filesystem::path &part) { return part == ".."; });
}

inline std::vector<std::filesystem::path> parts(const std::filesystem::path &relative)
{
    std::vector<std::filesystem::path> result;
    for(const std::filesystem::path &part : relative)
        if(part != ".")
            result.push_back(part);
    return result;
}

inline native_file_result stream(fd_owner file) noexcept
{
    struct stat state{};
    errno = 0;
    if(::fstat(file.get(), &state) != 0)
        return unexpected<text_read_failure>(native_failure(text_read_failure_kind::status, operation_kind::status, native_crt_error()));
    if(!S_ISREG(state.st_mode))
        return unexpected<text_read_failure>(native_failure(text_read_failure_kind::non_regular, operation_kind::status, {}));
    errno             = 0;
    std::FILE *opened = ::fdopen(file.get(), "rb");
    if(opened == nullptr)
        return unexpected<text_read_failure>(native_failure(text_read_failure_kind::open, operation_kind::open, native_crt_error()));
    (void)file.release();
    return opened;
}

}

// A caller-named top-level path has no root to escape, so this open follows a final symlink the
// way the Windows CRT does; the fstat below still decides on the opened descriptor, never a name.
// Root-relative opens keep O_NOFOLLOW, where a link can leave the authorized root.
inline native_file_result native_open(const std::filesystem::path &path) noexcept
{
    errno = 0;
    native::fd_owner file{::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC)};
    if(file.get() < 0)
        return unexpected<text_read_failure>(native_failure(text_read_failure_kind::open, operation_kind::open, native_crt_error()));
    return native::stream(std::move(file));
}

inline native_file_result native_open_under(const std::filesystem::path &root, const std::filesystem::path &relative) noexcept
{
    if(!native::valid_relative(relative))
        return unexpected<text_read_failure>(native_failure(text_read_failure_kind::open, operation_kind::open, make_error_code(std::errc::invalid_argument)));
    errno = 0;
    std::vector<native::fd_owner> handles;
    handles.emplace_back(::open(root.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
    if(handles.back().get() < 0)
        return unexpected<text_read_failure>(native_failure(text_read_failure_kind::open, operation_kind::open, native_crt_error()));
    const std::vector<std::filesystem::path> components = native::parts(relative);
    if(components.empty())
        return unexpected<text_read_failure>(native_failure(text_read_failure_kind::open, operation_kind::open, make_error_code(std::errc::invalid_argument)));
    for(std::size_t index = 0; index < components.size(); ++index)
    {
        const bool final = index + 1 == components.size();
        const int flags  = O_RDONLY | O_CLOEXEC | O_NOFOLLOW | (final ? O_NONBLOCK : O_DIRECTORY);
        errno            = 0;
        handles.emplace_back(::openat(handles.back().get(), components[index].c_str(), flags));
        if(handles.back().get() < 0)
            return unexpected<text_read_failure>(native_failure(text_read_failure_kind::open, operation_kind::open, native_crt_error()));
    }
    return native::stream(std::move(handles.back()));
}

}

#endif

#endif
