#ifndef HPP_GUARD_MEIOS_CORE_URDF_YAML_PACKAGE_RESOURCE_H
#define HPP_GUARD_MEIOS_CORE_URDF_YAML_PACKAGE_RESOURCE_H

#include "yaml_resource.h"

#include "meios/io/text_reader.h"
#include "meios/io/source_stack.h"
#include "meios/io/source_lookup.h"
#include "meios/io/resolved_asset.h"
#include "meios/io/text_reader_operations.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"
#include "meios/diagnostic/source_location.h"

#include <string>
#include <cstddef>
#include <utility>
#include <optional>
#include <string_view>

namespace meios::detail::yaml_package
{

constexpr std::string_view package_prefix = "package://";
constexpr std::string_view find_prefix    = "$(find ";

struct package_ref
{
    std::string package;
    std::string relative;
};

inline bool matches(std::string_view spec)
{
    return spec.starts_with(package_prefix) || spec.starts_with(find_prefix);
}

inline bool separator(char c)
{
    return c == '/' || c == '\\';
}

// Both splitters refuse what the asset grammar refuses: no divider, an empty package half, an
// empty relative half, and a relative half opening on a further separator.
inline std::optional<package_ref> package_split(std::string_view spec)
{
    spec.remove_prefix(package_prefix.size());
    const std::size_t slash = spec.find('/');
    if(slash == std::string_view::npos || slash == 0 || slash + 1 == spec.size() || spec[slash + 1] == '/')
        return std::nullopt;
    return package_ref{std::string(spec.substr(0, slash)), std::string(spec.substr(slash + 1))};
}

inline std::optional<package_ref> find_split(std::string_view spec)
{
    spec.remove_prefix(find_prefix.size());
    const std::size_t close = spec.find(')');
    if(close == std::string_view::npos || close == 0)
        return std::nullopt;
    const std::string_view rest = spec.substr(close + 1);
    if(rest.size() < 2 || !separator(rest.front()) || separator(rest[1]))
        return std::nullopt;
    return package_ref{std::string(spec.substr(0, close)), std::string(rest.substr(1))};
}

inline void report_failure(std::string_view subject, const operation_failure &cause, log_sink &log)
{
    log.log(level::error, diagnostic_code::cannot_open, source_location{}, cause,
            "cannot " + std::string(to_string(cause.operation)) + " resource \"" + std::string(subject) + "\": " + cause.native.message());
}

inline void report_failure(std::string_view subject, const text_read_failure &failure, std::size_t maximum, log_sink &log)
{
    if(failure.kind == text_read_failure_kind::too_large)
        return report_too_large(subject, maximum, log);
    log.log(level::error, diagnostic_code::cannot_open, source_location{}, failure.cause, "cannot read resource \"" + std::string(subject) + "\": " + read_failure_reason(failure));
}

inline std::optional<std::string> consume(text_read_result text, std::string_view subject, std::size_t maximum, log_sink &log)
{
    if(text)
        return std::move(*text);
    report_failure(subject, text.error(), maximum, log);
    return std::nullopt;
}

inline std::optional<resolved_asset> locate(std::string_view spec, const package_ref &ref, source_stack &sources, log_sink &log)
{
    source_lookup_result hit = sources.try_locate(ref.package, ref.relative, log);
    if(!hit)
    {
        report_failure(spec, hit.error(), log);
        return std::nullopt;
    }
    if(!*hit)
    {
        log.log(level::error, diagnostic_code::unresolved_asset, source_location{}, "could not resolve resource \"" + std::string(spec) + '"');
        return std::nullopt;
    }
    return std::move(**hit);
}

inline text_read_result read(const resolved_asset &asset, const text_reader_operations &operations, std::size_t maximum)
{
    if(asset.source_root() && asset.source_relative())
        return read_text_file_under(*asset.source_root(), *asset.source_relative(), operations, maximum);
    return read_text_file(asset.path(), operations, maximum);
}

inline std::optional<std::string> fetch(std::string_view spec, source_stack &sources, log_sink &log, const text_reader_operations &operations, std::size_t maximum)
{
    const std::optional<package_ref> ref = spec.starts_with(package_prefix) ? package_split(spec) : find_split(spec);
    if(!ref)
    {
        log.log(level::error, diagnostic_code::malformed_asset_uri, source_location{}, "malformed package resource spec \"" + std::string(spec) + '"');
        return std::nullopt;
    }
    const std::optional<resolved_asset> hit = locate(spec, *ref, sources, log);
    return hit ? consume(read(*hit, operations, maximum), hit->path().string(), maximum, log) : std::nullopt;
}

}

#endif
