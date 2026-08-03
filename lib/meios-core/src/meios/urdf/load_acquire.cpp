#include "load_acquire.h"

#include "meios/io/text_reader.h"
#include "meios/io/text_reader_operations.h"

#include "meios/diagnostic/load_error.h"
#include "meios/diagnostic/diagnostic_code.h"
#include "meios/diagnostic/source_location.h"
#include "meios/diagnostic/captured_diagnostic.h"

#include <string>
#include <vector>
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

// The returned error is where the refusal lives: logging it here as well would give one
// acquisition failure two owners, and every caller that relays a returned load_error would
// then print it a second time. The record is placed in the error's own audit list instead.
expected<std::string, load_error> acquire_load_text(const std::filesystem::path &path, capture_window &window, const text_reader_operations &operations)
{
    text_read_result result = detail::read_text_file(path, operations);
    if(result)
        return std::move(*result);

    const text_read_failure &failure = result.error();
    const source_location location{path, 0, 0};
    const std::string message                = failure_message(path, failure);
    std::vector<captured_diagnostic> records = window.records();
    records.push_back({diagnostic_code::cannot_open, location, message, failure.cause});
    return unexpected<load_error>({location, message, diagnostic_code::cannot_open, std::move(records), failure.cause});
}

expected<std::string, load_error> acquire_load_text(const std::filesystem::path &path, capture_window &window)
{
    return acquire_load_text(path, window, default_text_reader_operations());
}

}
