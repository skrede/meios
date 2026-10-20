#include "yaml_sweep.h"
#include "yaml_package_resource.h"

#include "meios/io/text_reader.h"
#include "meios/io/directory_source.h"
#include "meios/io/beside_containment.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"
#include "meios/diagnostic/source_location.h"

#include <string>
#include <utility>
#include <optional>
#include <filesystem>
#include <string_view>
#include <system_error>

namespace meios::detail::yaml_sweep
{

void remember(sweep &seen, const text_read_failure &failure)
{
    if(!seen.first_failure)
        seen.first_failure = failure;
}

bool present(const std::filesystem::path &path, sweep &seen)
{
    std::error_code error;
    const std::filesystem::file_status status = std::filesystem::status(path, error);
    const bool absent                         = error == std::errc::no_such_file_or_directory || error == std::errc::not_a_directory;
    if(error && !absent)
        remember(seen, {text_read_failure_kind::status, {operation_kind::status, error}});
    else if(absent || !std::filesystem::exists(status))
        seen.absent = true;
    return !error && std::filesystem::exists(status);
}

// A root holding the resource past the read ceiling ends the sweep: the copy it holds is the one
// the spec names, so a later root's copy must not stand in for it, and the crossing outranks any
// native failure an earlier root recovered from.
std::optional<text_resource_result> settle(text_read_result text, sweep &seen)
{
    if(text)
        return text_resource_result{std::move(*text)};
    if(text.error().kind == text_read_failure_kind::too_large)
        return text_resource_result{unexpected<text_resource_failure>(text_resource_failure::exhausted)};
    remember(seen, text.error());
    return std::nullopt;
}

std::optional<text_resource_result> read_contained(const contained_read_result &contained, sweep &seen)
{
    if(!contained)
        remember(seen, {text_read_failure_kind::status, contained.error()});
    else if(!*contained)
        seen.uncontained = true;
    else if(const contained_read &found = **contained; present(found.real, seen))
        return settle(read_text_file_under(found.root, found.real.lexically_relative(found.root), seen.operations, seen.maximum), seen);
    return std::nullopt;
}

std::optional<text_resource_result> from_root(std::string_view spec, const std::filesystem::path &root, sweep &seen)
{
    const std::filesystem::path base = absolute_base(root);
    return read_contained(contain_under(base, base / std::filesystem::path(spec)), seen);
}

bool passes_link(const std::filesystem::path &document, std::string_view spec, sweep &seen)
{
    const expected<std::optional<std::filesystem::path>, operation_failure> walked = first_beside_link(document, std::filesystem::path(spec));
    if(!walked)
        remember(seen, {text_read_failure_kind::status, walked.error()});
    return !walked || *walked;
}

text_resource_result refuse_uncontained(std::string_view spec, log_sink &log)
{
    log.log(level::error, diagnostic_code::uncontained_asset, source_location{}, "refused resource \"" + std::string(spec) + "\" outside containment roots");
    return unexpected<text_resource_failure>(text_resource_failure::uncontained);
}

// A determined native cause outranks both verdicts; between the two, absence wins, because a root
// that holds the place for the resource has answered the containment question the other one raised.
text_resource_result conclude_sweep(std::string_view spec, const sweep &seen, log_sink &log)
{
    if(seen.first_failure)
    {
        yaml_package::report_failure(spec, *seen.first_failure, log);
        return unexpected<text_resource_failure>(text_resource_failure::unreachable);
    }
    if(seen.uncontained && !seen.absent)
        return refuse_uncontained(spec, log);
    log.log(level::error, diagnostic_code::unresolved_asset, source_location{}, "could not resolve resource \"" + std::string(spec) + '"');
    return unexpected<text_resource_failure>(text_resource_failure::unreachable);
}

}
