#include "beside_containment.h"

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

resolved_asset admitted(const detail::contained_read &read)
{
    return resolved_asset{read.real, read.root, read.real.lexically_relative(read.root)};
}

bool is_absent(const std::error_code &error)
{
    return error == std::errc::no_such_file_or_directory || error == std::errc::not_a_directory;
}

}

std::filesystem::path detail::absolute_base(const std::filesystem::path &root)
{
    if(root.empty())
        return root;
    std::error_code ec;
    const std::filesystem::path full = std::filesystem::absolute(root, ec);
    return ec ? root : full;
}

std::optional<std::filesystem::path> detail::contained_under(const std::filesystem::path &root, const std::filesystem::path &candidate)
{
    contained_path_result result = try_contained_under(root, candidate);
    return result ? std::move(*result) : std::nullopt;
}

detail::contained_path_result detail::try_contained_under(const std::filesystem::path &root, const std::filesystem::path &candidate)
{
    contained_read_result decided = contain_under(root, candidate);
    if(!decided)
        return unexpected<operation_failure>(decided.error());
    if(!*decided)
        return std::nullopt;
    return std::optional{std::move((*decided)->real)};
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

std::optional<resolved_asset> detail::locate_contained(const std::filesystem::path &root, std::string_view package, std::string_view relative, log_sink &log)
{
    const contained_read_result decided = contain_under(root, package_candidate(root, package, relative));
    if(!decided || !*decided)
    {
        log.log(level::error, diagnostic_code::uncontained_asset, source_location{}, reject_message(package, relative));
        return std::nullopt;
    }
    std::error_code ec;
    if(!std::filesystem::exists((*decided)->real, ec))
        return std::nullopt;
    return admitted(**decided);
}

directory_source::directory_source(std::filesystem::path root, log_sink &log)
        : m_root(detail::absolute_base(root))
        , m_log(log)
{
}

capability_descriptor directory_source::capabilities()
{
    return {source_kind::directory, false};
}

std::optional<resolved_asset> directory_source::locate(std::string_view package, std::string_view relative)
{
    return detail::locate_contained(m_root, package, relative, m_log.get());
}

source_lookup_result directory_source::try_locate(std::string_view package, std::string_view relative)
{
    const detail::contained_read_result decided = detail::contain_under(m_root, package_candidate(m_root, package, relative));
    if(!decided)
        return unexpected<operation_failure>(decided.error());
    if(!*decided)
    {
        m_log.get().log(level::error, diagnostic_code::uncontained_asset, source_location{}, reject_message(package, relative));
        return std::optional<resolved_asset>{};
    }
    std::error_code error;
    const std::filesystem::file_status status = std::filesystem::status((*decided)->real, error);
    if(is_absent(error))
        return std::optional<resolved_asset>{};
    if(error)
        return unexpected<operation_failure>({operation_kind::status, error});
    if(!std::filesystem::exists(status))
        return std::optional<resolved_asset>{};
    return std::optional{admitted(**decided)};
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
