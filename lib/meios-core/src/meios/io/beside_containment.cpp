#include "beside_containment.h"

#include <optional>
#include <filesystem>
#include <system_error>

namespace meios::detail
{

namespace
{

bool escapes(const std::filesystem::path &root, const std::filesystem::path &real)
{
    const std::filesystem::path relative = real.lexically_relative(root);
    return relative.empty() || *relative.begin() == std::filesystem::path("..");
}

bool absent(const std::filesystem::file_status &status, const std::error_code &error)
{
    return status.type() == std::filesystem::file_type::not_found || error == std::errc::no_such_file_or_directory || error == std::errc::not_a_directory;
}

}

// A containment decision is only meaningful between two absolute paths: canonicalizing a
// path with no existing prefix succeeds and leaves it relative, at which point the escape
// test measures it against the process working directory rather than against a root.
contained_read_result contain_under(const std::filesystem::path &root, const std::filesystem::path &candidate)
{
    if(!root.is_absolute() || !candidate.is_absolute())
        return std::optional<contained_read>{};
    std::error_code error;
    const std::filesystem::path base = std::filesystem::weakly_canonical(root, error);
    if(error)
        return unexpected<operation_failure>({operation_kind::canonicalize, error});
    const std::filesystem::path real = std::filesystem::weakly_canonical(candidate, error);
    if(error)
        return unexpected<operation_failure>({operation_kind::canonicalize, error});
    if(escapes(base, real))
        return std::optional<contained_read>{};
    return std::optional{contained_read{real, base}};
}

contained_read_result contain_beside(const std::filesystem::path &document, const std::filesystem::path &spec)
{
    const std::filesystem::path named     = document.parent_path();
    const std::filesystem::path candidate = named / spec;
    contained_read_result as_named        = contain_under(named, candidate);
    if(!as_named || *as_named)
        return as_named;
    std::error_code error;
    const std::filesystem::path real = std::filesystem::weakly_canonical(document, error);
    if(error)
        return unexpected<operation_failure>({operation_kind::canonicalize, error});
    return contain_under(real.parent_path(), candidate);
}

std::optional<std::filesystem::path> beside_link(const std::filesystem::path &document, const std::filesystem::path &spec)
{
    const std::filesystem::path normal = spec.lexically_normal();
    if(spec.has_root_path() || normal.empty() || *normal.begin() == std::filesystem::path(".."))
        return std::nullopt;
    return document.parent_path() / spec;
}

expected<std::optional<std::filesystem::path>, operation_failure> first_beside_link(const std::filesystem::path &document, const std::filesystem::path &spec)
{
    if(!beside_link(document, spec))
        return std::optional<std::filesystem::path>{};
    std::filesystem::path prefix = document.parent_path();
    for(const std::filesystem::path &element : spec)
    {
        prefix /= element;
        std::error_code error;
        const std::filesystem::file_status status = std::filesystem::symlink_status(prefix, error);
        if(absent(status, error))
            return std::optional<std::filesystem::path>{};
        if(error)
            return unexpected<operation_failure>({operation_kind::status, error});
        if(std::filesystem::is_symlink(status))
            return std::optional{prefix};
    }
    return std::optional<std::filesystem::path>{};
}

}
