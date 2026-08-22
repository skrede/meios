#ifndef HPP_GUARD_MEIOS_YAML_SCALAR_RESOLVER_H
#define HPP_GUARD_MEIOS_YAML_SCALAR_RESOLVER_H

#include "meios/xacro/value.h"
#include "meios/xacro/yaml_parser_handle.h"

#include <string>
#include <utility>
#include <optional>
#include <string_view>

namespace meios::detail
{

// A refusal names the construct and its category and nothing else: the reader owns the
// position and the diagnostic, which leaves the resolver a pure function of a scalar and
// its tag and therefore drivable straight from the recorded measurements.
struct scalar_result
{
    std::optional<value> resolved;
    std::string refusal;
    yaml_failure failure;
};

inline scalar_result yields(value resolved)
{
    return scalar_result{ std::move(resolved), std::string(), yaml_failure::none };
}

inline scalar_result declines(std::string construct)
{
    return scalar_result{ std::nullopt, std::move(construct), yaml_failure::unsupported };
}

inline scalar_result rejects(std::string construct)
{
    return scalar_result{ std::nullopt, std::move(construct), yaml_failure::refused };
}

scalar_result resolve_scalar(std::string_view text, std::string_view tag);

scalar_result resolve_unit_tag(std::string_view text, std::string_view tag);

}

#endif
