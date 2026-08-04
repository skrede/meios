#ifndef HPP_GUARD_MEIOS_IO_TEXT_READER_H
#define HPP_GUARD_MEIOS_IO_TEXT_READER_H

#include "meios/expected.h"

#include "meios/diagnostic/operation_failure.h"

#include <string>
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

text_read_result read_text_file(const std::filesystem::path &path);
text_read_result read_text_file_under(const std::filesystem::path &root, const std::filesystem::path &relative);

}

#endif
