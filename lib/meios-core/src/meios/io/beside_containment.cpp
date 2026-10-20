#include "beside_containment.h"

#include "meios/io/directory_source.h"

#include <span>
#include <utility>
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

std::optional<contained_read> admit(const std::filesystem::path &directory, const std::filesystem::path &candidate, std::optional<operation_failure> &retained)
{
    contained_read_result decided = contain_under(directory, candidate);
    if(decided)
        return std::move(*decided);
    if(!retained)
        retained = decided.error();
    return std::nullopt;
}

std::optional<contained_read> admit_real(const std::filesystem::path &document, const std::filesystem::path &candidate, std::optional<operation_failure> &retained)
{
    std::error_code error;
    const std::filesystem::path real = std::filesystem::weakly_canonical(document, error);
    if(!error)
        return admit(real.parent_path(), candidate, retained);
    if(!retained)
        retained = operation_failure{operation_kind::canonicalize, error};
    return std::nullopt;
}

// The reference opens the spelling as written, so an element that is not a directory refuses it, where
// weakly_canonical stops at that element and reduces the rest by its spelling [fs.op.weakly.canonical].
std::optional<operation_failure> refuse_non_directory(const std::filesystem::path &joined)
{
    std::error_code error;
    const std::filesystem::file_status status = std::filesystem::status(joined, error);
    if(status.type() == std::filesystem::file_type::not_found && error == std::errc::not_a_directory)
        return operation_failure{operation_kind::status, error};
    return std::nullopt;
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

contained_read_result contain_beside(const std::filesystem::path &document, const std::filesystem::path &spec, std::span<const std::filesystem::path> roots)
{
    const std::filesystem::path candidate = document.parent_path() / spec;
    if(!document.empty() && candidate.is_absolute())
        if(std::optional<operation_failure> refused = refuse_non_directory(candidate))
            return unexpected<operation_failure>(*refused);
    std::optional<operation_failure> retained;
    std::optional<contained_read> found;
    if(!document.empty())
        found = admit(document.parent_path(), candidate, retained);
    if(!found && !document.empty())
        found = admit_real(document, candidate, retained);
    for(auto root = roots.begin(); !found && root != roots.end(); ++root)
        found = admit(absolute_base(*root), candidate, retained);
    if(found || !retained)
        return found;
    return unexpected<operation_failure>(*retained);
}

bool through_non_directory(const operation_failure &failure)
{
    return failure.operation == operation_kind::status && failure.native == std::errc::not_a_directory;
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
