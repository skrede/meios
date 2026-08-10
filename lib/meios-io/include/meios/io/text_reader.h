#ifndef HPP_GUARD_MEIOS_IO_TEXT_READER_H
#define HPP_GUARD_MEIOS_IO_TEXT_READER_H

#include "meios/expected.h"

#include "meios/diagnostic/operation_failure.h"

#include <string>
#include <cstddef>
#include <filesystem>

namespace meios
{

enum class text_read_failure_kind
{
    status,
    non_regular,
    open,
    read,
    close,
    too_large,
};

struct text_read_failure
{
    text_read_failure(text_read_failure_kind initial_kind, operation_failure initial_cause)
            : kind(initial_kind)
            , cause(initial_cause)
    {
    }

    text_read_failure_kind kind;
    operation_failure cause;
};

using text_read_result = expected<std::string, text_read_failure>;

namespace detail
{

// A refusal by kind carries no native code, so naming the classification is what keeps the
// message from reporting a failure whose stated reason is that nothing went wrong.
inline std::string read_failure_reason(const text_read_failure &failure)
{
    if(failure.kind == text_read_failure_kind::non_regular)
        return "not a regular file";
    if(failure.kind == text_read_failure_kind::too_large)
        return "larger than the maximum this read accepts";
    return std::string(to_string(failure.cause.operation)) + " failed: " + failure.cause.native.message();
}

}

text_read_result read_text_file(const std::filesystem::path &path);
// A read that stops at a ceiling rather than growing to the file: the maximum bounds what the
// reader accumulates, so peak allocation follows the caller's budget and not the size an author
// chose. A file past it refuses without ever being held whole.
text_read_result read_text_file(const std::filesystem::path &path, std::size_t maximum);
text_read_result read_text_file_under(const std::filesystem::path &root, const std::filesystem::path &relative);

}

#endif
