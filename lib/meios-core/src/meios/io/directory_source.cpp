#include "meios/io/directory_source.h"

#include "meios/expected.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/diagnostic_code.h"
#include "meios/diagnostic/source_location.h"
#include "meios/diagnostic/operation_failure.h"

#include <string>
#include <utility>
#include <optional>
#include <filesystem>
#include <string_view>
#include <system_error>

namespace meios
{

namespace
{

bool escapes_root(const std::filesystem::path &root, const std::filesystem::path &candidate)
{
    std::filesystem::path relative = candidate.lexically_relative(root);
    if(relative.empty())
        return true;
    return *relative.begin() == std::filesystem::path("..");
}

std::string reject_message(std::string_view package, std::string_view relative)
{
    return "rejected package path \"" + std::string(package) + '/' + std::string(relative) + "\" that escapes the configured source root";
}

std::filesystem::path package_candidate(const std::filesystem::path &root, std::string_view package, std::string_view relative)
{
    std::filesystem::path candidate = root / std::string(package);
    if(!relative.empty())
        candidate /= std::string(relative);
    return candidate;
}

std::filesystem::path package_request(std::string_view package, std::string_view relative)
{
    std::filesystem::path request = std::string(package);
    if(!relative.empty())
        request /= std::string(relative);
    return request;
}

bool is_absent(const std::error_code &error)
{
    return error == std::errc::no_such_file_or_directory || error == std::errc::not_a_directory;
}

}

// A containment decision is only meaningful between two absolute paths: canonicalizing a
// path with no existing prefix succeeds and leaves it relative, at which point the escape
// test measures it against the process working directory rather than against a root.
std::optional<std::filesystem::path> detail::contained_under(const std::filesystem::path &root, const std::filesystem::path &candidate)
{
    contained_path_result result = try_contained_under(root, candidate);
    return result ? std::move(*result) : std::nullopt;
}

detail::contained_path_result detail::try_contained_under(const std::filesystem::path &root, const std::filesystem::path &candidate)
{
    std::error_code ec;
    const std::filesystem::path base = std::filesystem::weakly_canonical(root, ec);
    if(ec)
        return unexpected<operation_failure>({operation_kind::canonicalize, ec});
    if(!base.is_absolute())
        return std::nullopt;
    const std::filesystem::path real = std::filesystem::weakly_canonical(candidate, ec);
    if(ec)
        return unexpected<operation_failure>({operation_kind::canonicalize, ec});
    if(!real.is_absolute() || escapes_root(base, real))
        return std::nullopt;
    return std::optional{real};
}

std::optional<std::filesystem::path> detail::contained_candidate(const std::filesystem::path &root, std::string_view package, std::string_view relative, log_sink &log)
{
    const std::filesystem::path candidate     = package_candidate(root, package, relative);
    std::optional<std::filesystem::path> real = detail::contained_under(root, candidate);
    if(!real)
    {
        log.log(level::error, diagnostic_code::uncontained_asset, source_location{}, reject_message(package, relative));
        return std::nullopt;
    }
    return real;
}

directory_source::directory_source(std::filesystem::path root, log_sink &log)
        : m_root(std::move(root))
        , m_log(log)
{
}

capability_descriptor directory_source::capabilities()
{
    return {source_kind::directory, false};
}

std::optional<resolved_asset> directory_source::locate(std::string_view package, std::string_view relative)
{
    std::optional<std::filesystem::path> candidate = detail::contained_candidate(m_root, package, relative, m_log.get());
    std::error_code ec;
    if(!candidate || !std::filesystem::exists(*candidate, ec))
        return std::nullopt;
    return resolved_asset{*candidate, m_root, package_request(package, relative)};
}

source_lookup_result directory_source::try_locate(std::string_view package, std::string_view relative)
{
    detail::contained_path_result candidate = detail::try_contained_under(m_root, package_candidate(m_root, package, relative));
    if(!candidate)
        return unexpected<operation_failure>(candidate.error());
    if(!*candidate)
    {
        m_log.get().log(level::error, diagnostic_code::uncontained_asset, source_location{}, reject_message(package, relative));
        return std::optional<resolved_asset>{};
    }
    std::error_code error;
    const std::filesystem::file_status status = std::filesystem::status(**candidate, error);
    if(is_absent(error))
        return std::optional<resolved_asset>{};
    if(error)
        return unexpected<operation_failure>({operation_kind::status, error});
    if(!std::filesystem::exists(status))
        return std::optional<resolved_asset>{};
    return std::optional{resolved_asset{**candidate, m_root, package_request(package, relative)}};
}

std::optional<std::filesystem::path> directory_source::path_of(std::string_view package, std::string_view relative) const
{
    std::optional<std::filesystem::path> candidate = detail::contained_candidate(m_root, package, relative, m_log.get());
    std::error_code ec;
    if(!candidate || !std::filesystem::exists(*candidate, ec))
        return std::nullopt;
    return candidate;
}

}
