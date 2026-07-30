#include "meios/io/text_reader_operations.h"

#include <span>
#include <array>
#include <memory>
#include <string>
#include <cstddef>
#include <utility>
#include <filesystem>
#include <system_error>

namespace meios
{

namespace detail
{

namespace
{

text_read_result failure(text_read_failure_kind kind, operation_kind operation, std::error_code native)
{
    return unexpected<text_read_failure>({kind, {operation, native}});
}

std::error_code backend_error(std::error_code error)
{
    return error ? error : make_error_code(std::errc::io_error);
}

text_read_result read_open_file(std::unique_ptr<text_file> file)
{
    std::array<char, 64 * 1024> buffer{};
    std::string content;
    while(true)
    {
        const std::span<char> request = buffer;
        const std::size_t transferred = file->read(request);
        if(file->failed())
            return failure(text_read_failure_kind::read, operation_kind::read, backend_error(file->native_error()));
        if(transferred > request.size())
            return failure(text_read_failure_kind::read, operation_kind::read, make_error_code(std::errc::io_error));
        if(transferred == 0)
            break;
        content.append(buffer.data(), transferred);
    }
    if(!file->close())
        return failure(text_read_failure_kind::close, operation_kind::close, backend_error(file->native_error()));
    return content;
}

}

text_read_result read_text_file(const std::filesystem::path &path, const text_reader_operations &operations)
{
    text_open_result opened = operations.open_checked(path);
    if(!opened)
        return unexpected<text_read_failure>(opened.error());
    return read_open_file(std::move(*opened));
}

text_read_result read_text_file_under(const std::filesystem::path &root, const std::filesystem::path &relative, const text_reader_operations &operations)
{
    text_open_result opened = operations.open_under(root, relative);
    if(!opened)
        return unexpected<text_read_failure>(opened.error());
    return read_open_file(std::move(*opened));
}

text_read_result read_text_file_under(const std::filesystem::path &root, const std::filesystem::path &relative)
{
    return read_text_file_under(root, relative, default_text_reader_operations());
}

}

text_read_result read_text_file(const std::filesystem::path &path)
{
    return detail::read_text_file(path, detail::default_text_reader_operations());
}

}
