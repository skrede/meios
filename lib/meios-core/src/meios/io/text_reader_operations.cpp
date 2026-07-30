#include "meios/io/text_reader_operations.h"

#include <cerrno>
#include <cstdio>
#include <memory>
#include <utility>
#include <system_error>

namespace meios::detail
{

namespace
{

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
            m_error  = {errno, std::system_category()};
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
        m_error  = {errno, std::system_category()};
        return false;
    }

private:
    bool m_failed;
    std::FILE *m_file;
    std::error_code m_error;
};

class native_text_reader_operations final : public text_reader_operations
{
public:
    text_status_result status(const std::filesystem::path &path) const noexcept override
    {
        std::error_code error;
        const std::filesystem::file_status result = std::filesystem::status(path, error);
        if(error)
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
            return unexpected<std::error_code>({errno, std::system_category()});
        return std::unique_ptr<text_file>(std::make_unique<native_text_file>(file));
    }
};

}

const text_reader_operations &default_text_reader_operations() noexcept
{
    static const native_text_reader_operations operations;
    return operations;
}

}
