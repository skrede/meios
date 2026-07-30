#include "meios/io/text_reader_operations.h"

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

text_read_result read_open_file(std::unique_ptr<text_file> file)
{
    std::array<char, 64 * 1024> buffer{};
    std::string content;
    while(true)
    {
        const std::size_t transferred = file->read(buffer);
        if(file->failed())
            return failure(text_read_failure_kind::read, operation_kind::read, file->native_error());
        if(transferred == 0)
            break;
        content.append(buffer.data(), transferred);
    }
    if(!file->close())
        return failure(text_read_failure_kind::close, operation_kind::close, file->native_error());
    return content;
}

}

text_read_result read_text_file(const std::filesystem::path &path, const text_reader_operations &operations)
{
    const text_status_result state = operations.status(path);
    if(!state)
        return failure(text_read_failure_kind::status, operation_kind::status, state.error());
    if(!std::filesystem::is_regular_file(*state))
        return failure(text_read_failure_kind::non_regular, operation_kind::status, {});

    text_file_result opened = operations.open(path);
    if(!opened)
        return failure(text_read_failure_kind::open, operation_kind::open, opened.error());
    return read_open_file(std::move(*opened));
}

}

text_read_result read_text_file(const std::filesystem::path &path)
{
    return detail::read_text_file(path, detail::default_text_reader_operations());
}

}
