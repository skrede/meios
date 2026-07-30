#include "load_acquire.h"

#include "meios/io/text_reader.h"
#include "meios/io/text_reader_operations.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/load_error.h"
#include "meios/diagnostic/diagnostic_code.h"
#include "meios/diagnostic/source_location.h"

#include <string>
#include <utility>
#include <filesystem>

namespace meios::detail
{

namespace
{

std::string failure_message(const std::filesystem::path &path, const text_read_failure &failure)
{
    if(failure.kind == text_read_failure_kind::non_regular)
        return "input file \"" + path.string() + "\" is not a regular file";
    return "cannot " + std::string(to_string(failure.cause.operation)) + " input file \"" + path.string() + "\": " + failure.cause.native.message();
}

}

expected<std::string, load_error> acquire_load_text(const std::filesystem::path &path, capture_window &window, const text_reader_operations &operations)
{
    text_read_result result = detail::read_text_file(path, operations);
    if(result)
        return std::move(*result);

    const text_read_failure &failure = result.error();
    const source_location location{path, 0, 0};
    const std::string message = failure_message(path, failure);
    window.log().log(level::error, diagnostic_code::cannot_open, location, failure.cause, message);
    return unexpected<load_error>({location, message, diagnostic_code::cannot_open, window.records(), failure.cause});
}

expected<std::string, load_error> acquire_load_text(const std::filesystem::path &path, capture_window &window)
{
    return acquire_load_text(path, window, default_text_reader_operations());
}

}
