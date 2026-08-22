#include "meios/io/text_reader_operations.h"
#include "meios/io/text_reader_native.h"

#include <cerrno>
#include <cstdio>
#include <memory>
#include <utility>
#include <optional>
#include <system_error>

namespace meios::detail
{
namespace
{

text_read_failure failure(text_read_failure_kind kind, operation_kind operation, std::error_code error)
{
    return {kind, {operation, error}};
}

std::error_code crt_error() noexcept
{
    return errno == 0 ? make_error_code(std::errc::io_error) : std::error_code{errno, std::generic_category()};
}

// An absent path is left to the open leg, which owns the native cause a caller reads for it.
std::optional<text_read_failure> status_refusal(const text_status_result &state)
{
    if(!state)
        return failure(text_read_failure_kind::status, operation_kind::status, state.error());
    if(std::filesystem::exists(*state) && !std::filesystem::is_regular_file(*state))
        return failure(text_read_failure_kind::non_regular, operation_kind::status, {});
    return std::nullopt;
}

class native_text_file final : public text_file
{
public:
    explicit native_text_file(std::FILE *file)
            : m_failed(false)
            , m_file(file)
            , m_error()
    {
    }

    ~native_text_file() override
    {
        if(m_file != nullptr)
            (void)close();
    }

    std::size_t read(std::span<char> buffer) noexcept override
    {
        errno                         = 0;
        const std::size_t transferred = std::fread(buffer.data(), 1, buffer.size(), m_file);
        if(std::ferror(m_file) != 0)
        {
            m_failed = true;
            m_error  = crt_error();
        }
        return transferred;
    }

    bool failed() const noexcept override
    {
        return m_failed;
    }

    std::error_code native_error() const noexcept override
    {
        return m_error;
    }

    bool close() noexcept override
    {
        errno           = 0;
        std::FILE *file = std::exchange(m_file, nullptr);
        if(std::fclose(file) == 0)
            return true;
        m_failed = true;
        m_error  = crt_error();
        return false;
    }

private:
    bool m_failed;
    std::FILE *m_file;
    std::error_code m_error;
};

text_open_result wrap(native_file_result opened)
{
    if(!opened)
        return unexpected<text_read_failure>(opened.error());
    return std::unique_ptr<text_file>(std::make_unique<native_text_file>(*opened));
}

class native_text_reader_operations final : public text_reader_operations
{
public:
    // Implementations disagree on whether an absent path also sets the error code, so the
    // determinate not_found the standard names is answered as a status, never as a failure.
    text_status_result status(const std::filesystem::path &path) const noexcept override
    {
        std::error_code error;
        const std::filesystem::file_status result = std::filesystem::status(path, error);
        if(error && result.type() != std::filesystem::file_type::not_found)
            return unexpected<std::error_code>(error);
        return result;
    }

    text_file_result open(const std::filesystem::path &path) const noexcept override
    {
        errno = 0;
#if defined(_WIN32)
        std::FILE *file = _wfopen(path.c_str(), L"rb");
#else
        std::FILE *file = std::fopen(path.c_str(), "rb");
#endif
        if(file == nullptr)
            return unexpected<std::error_code>(crt_error());
        return std::unique_ptr<text_file>(std::make_unique<native_text_file>(file));
    }

    // The Windows CRT refuses a directory with EACCES at the open itself, indistinguishably from a
    // denied file, so the kind is settled from the status before any descriptor is asked for.
    text_open_result open_checked(const std::filesystem::path &path) const noexcept override
    {
        if(const std::optional<text_read_failure> refused = status_refusal(status(path)))
            return unexpected<text_read_failure>(*refused);
        return wrap(native_open(path));
    }

    text_open_result open_under(const std::filesystem::path &root, const std::filesystem::path &relative) const noexcept override
    {
        return wrap(native_open_under(root, relative));
    }
};

}

text_open_result text_reader_operations::open_checked(const std::filesystem::path &path) const noexcept
{
    if(const std::optional<text_read_failure> refused = status_refusal(status(path)))
        return unexpected<text_read_failure>(*refused);
    text_file_result opened = open(path);
    if(!opened)
        return unexpected<text_read_failure>(failure(text_read_failure_kind::open, operation_kind::open, opened.error() ? opened.error() : make_error_code(std::errc::io_error)));
    return std::move(*opened);
}

text_open_result text_reader_operations::open_under(const std::filesystem::path &, const std::filesystem::path &) const noexcept
{
    return unexpected<text_read_failure>(failure(text_read_failure_kind::open, operation_kind::open, make_error_code(std::errc::operation_not_supported)));
}

const text_reader_operations &default_text_reader_operations() noexcept
{
    static const native_text_reader_operations operations;
    return operations;
}

}
