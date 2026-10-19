#include "beside_containment.h"

#include "meios/io/directory_source.h"

#include <optional>
#include <filesystem>
#include <system_error>

namespace meios::detail
{

contained_read_result contain_under(const std::filesystem::path &root, const std::filesystem::path &candidate)
{
    if(!root.is_absolute())
        return std::optional<contained_read>{};
    std::error_code error;
    const std::filesystem::path base = std::filesystem::weakly_canonical(root, error);
    if(error)
        return unexpected<operation_failure>({operation_kind::canonicalize, error});
    const contained_path_result contained = try_contained_under(base, candidate);
    if(!contained)
        return unexpected<operation_failure>(contained.error());
    if(!*contained)
        return std::optional<contained_read>{};
    return std::optional{contained_read{**contained, base}};
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

}
