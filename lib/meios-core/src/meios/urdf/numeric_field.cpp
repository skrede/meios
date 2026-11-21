#include "urdf_detail.h"

#include "meios/xacro/detail/numeric.h"

#include "meios/diagnostic/diagnostic_code.h"

#include <cmath>
#include <string>
#include <cstddef>
#include <string_view>

namespace meios::detail
{

namespace
{

std::string quoted(std::string_view field)
{
    return "'" + std::string(field) + "'";
}

// The text urdfdom's stream extraction reads a number from: past the classic locale's white space
// and one plus sign that no second sign follows.
std::string_view extracted(std::string_view text)
{
    const std::size_t start = text.find_first_not_of(" \t\n\v\f\r");
    text = start == std::string_view::npos ? std::string_view{} : text.substr(start);
    if(text.starts_with('+') && !text.substr(1).starts_with('+') && !text.substr(1).starts_with('-'))
        text.remove_prefix(1);
    return text;
}

}

bool read_finite(std::string_view text, double &out, parse_context &ctx,
                 const source_location &loc, std::string_view field)
{
    bool parsed = false;
    const double value = parse_double(extracted(text), parsed);
    if(!parsed || !std::isfinite(value))
    {
        report_drop(ctx, loc, diagnostic_code::invalid_number,
                    quoted(field) + " is not a finite number");
        return false;
    }
    out = value;
    return true;
}

bool read_optional(pugi::xml_node node, const char *field, double &out, parse_context &ctx,
                   const source_location &loc)
{
    const pugi::xml_attribute declared = node.attribute(field);
    if(!declared)
        return true;
    return read_finite(declared.value(), out, ctx, loc, field);
}

bool read_required(pugi::xml_node node, const char *field, double &out, parse_context &ctx,
                   const source_location &loc)
{
    const pugi::xml_attribute declared = node.attribute(field);
    if(declared)
        return read_finite(declared.value(), out, ctx, loc, field);
    report_drop(ctx, loc, diagnostic_code::missing_required_field,
                std::string("<") + node.name() + "> declares no " + quoted(field));
    return false;
}

}
