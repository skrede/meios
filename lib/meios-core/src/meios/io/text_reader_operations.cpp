#include "meios/io/text_reader_operations.h"
#include "meios/io/text_reader_native.h"

#include <cerrno>
#include <cstdio>
#include <memory>
#include <utility>
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
    text_status_result status(const std::filesystem::path &path) const noexcept override
    {
        std::error_code error;
        const std::filesystem::file_status result = std::filesystem::status(path, error);
        return error ? text_status_result{unexpected<std::error_code>(error)} : text_status_result{result};
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

    text_open_result open_checked(const std::filesystem::path &path) const noexcept override
    {
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
    const text_status_result state = status(path);
    if(!state)
        return unexpected<text_read_failure>(failure(text_read_failure_kind::status, operation_kind::status, state.error()));
    if(!std::filesystem::is_regular_file(*state))
        return unexpected<text_read_failure>(failure(text_read_failure_kind::non_regular, operation_kind::status, {}));
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
