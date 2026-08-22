#ifndef HPP_GUARD_MEIOS_CORE_IO_TEXT_READER_OPERATIONS_H
#define HPP_GUARD_MEIOS_CORE_IO_TEXT_READER_OPERATIONS_H

#include "meios/io/text_reader.h"

#include "meios/expected.h"

#include <span>
#include <memory>
#include <cstddef>
#include <filesystem>
#include <system_error>

namespace meios::detail
{

class text_file
{
public:
    text_file()          = default;
    virtual ~text_file() = default;

    virtual std::size_t read(std::span<char> buffer) noexcept = 0;
    virtual bool failed() const noexcept                      = 0;
    virtual std::error_code native_error() const noexcept     = 0;
    virtual bool close() noexcept                             = 0;

protected:
    text_file(const text_file &)            = default;
    text_file &operator=(const text_file &) = default;
    text_file(text_file &&)                 = default;
    text_file &operator=(text_file &&)      = default;
};

using text_file_result   = expected<std::unique_ptr<text_file>, std::error_code>;
using text_open_result   = expected<std::unique_ptr<text_file>, text_read_failure>;
using text_status_result = expected<std::filesystem::file_status, std::error_code>;

class text_reader_operations
{
public:
    text_reader_operations()          = default;
    virtual ~text_reader_operations() = default;

    virtual text_status_result status(const std::filesystem::path &path) const noexcept = 0;
    virtual text_file_result open(const std::filesystem::path &path) const noexcept     = 0;
    virtual text_open_result open_checked(const std::filesystem::path &path) const noexcept;
    virtual text_open_result open_under(const std::filesystem::path &root, const std::filesystem::path &relative) const noexcept;

protected:
    text_reader_operations(const text_reader_operations &)            = default;
    text_reader_operations &operator=(const text_reader_operations &) = default;
    text_reader_operations(text_reader_operations &&)                 = default;
    text_reader_operations &operator=(text_reader_operations &&)      = default;
};

const text_reader_operations &default_text_reader_operations() noexcept;
text_read_result read_text_file(const std::filesystem::path &path, const text_reader_operations &operations);
text_read_result read_text_file(const std::filesystem::path &path, const text_reader_operations &operations, std::size_t maximum);
text_read_result read_text_file_under(const std::filesystem::path &root, const std::filesystem::path &relative, const text_reader_operations &operations);
text_read_result read_text_file_under(const std::filesystem::path &root, const std::filesystem::path &relative, const text_reader_operations &operations, std::size_t maximum);
text_read_result read_text_file_under(const std::filesystem::path &root, const std::filesystem::path &relative);

}

#endif
