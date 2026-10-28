#ifndef HPP_GUARD_MEIOS_TESTS_REFUSAL_CAUSES_H
#define HPP_GUARD_MEIOS_TESTS_REFUSAL_CAUSES_H

#include <meios/diagnostic/diagnostic_code.h>

#include <span>
#include <string>
#include <cstddef>
#include <optional>
#include <string_view>

namespace differential
{

// The inner class is the one the upstream exception wraps, or - where it wraps none.
struct refusal_cause
{
    std::string_view outer;
    std::string_view inner;
    std::string_view stem;
    meios::diagnostic_code code;
};

struct upstream_refusal
{
    std::string outer;
    std::string inner;
    std::string line;
};

// Each row maps an upstream cause to the code this side gives for the same cause. Where this side does not
// refuse that cause yet, the row names the code its closure will give: xacro_structural_error for a structural
// rule of the element grammar (attribute, name, block, macro, circular definition), undefined_property for an
// undefined name, expression_error for an evaluation failure inside a span, unresolved_include for a file that
// cannot be read, unresolved_find for a package not found, unresolved_arg for an undefined substitution argument,
// unknown_substitution for a substitution command that is unknown or cannot be carried out, unresolved_env for
// an unset environment variable and xacro_parse_error for malformed XML.
inline constexpr refusal_cause refusal_causes[]{
        {"XacroException", "-", "circular variable definition", meios::diagnostic_code::xacro_structural_error},
        {"XacroException", "-", "invalid expression: ", meios::diagnostic_code::expression_error},
        {"XacroException", "-", "invalid expression: ${", meios::diagnostic_code::unterminated_substitution},
        {"XacroException", "-", "invalid expression: $(", meios::diagnostic_code::unterminated_substitution},
        {"XacroException", "-", "variable filename is supported with in-order option only", meios::diagnostic_code::xacro_structural_error},
        {"XacroException", "-", "namespaces are supported with in-order option only", meios::diagnostic_code::xacro_structural_error},
        {"XacroException", "-", "Invalid use of macro name 'call'", meios::diagnostic_code::xacro_structural_error},
        {"XacroException", "-", "macro names must not contain '.'", meios::diagnostic_code::xacro_structural_error},
        {"XacroException", "-", "Property names must", meios::diagnostic_code::xacro_structural_error},
        {"XacroException", "-", "Property attributes default, value, and remove are mutually exclusive", meios::diagnostic_code::xacro_structural_error},
        {"XacroException", "-", "Undefined property to forward", meios::diagnostic_code::xacro_structural_error},
        {"XacroException", "-", "xacro:call is missing the 'macro' attribute", meios::diagnostic_code::xacro_structural_error},
        {"XacroException", "-", "unknown macro name", meios::diagnostic_code::xacro_structural_error},
        {"XacroException", "-", "Invalid parameter", meios::diagnostic_code::xacro_structural_error},
        {"XacroException", "-", "Not enough blocks", meios::diagnostic_code::xacro_structural_error},
        {"XacroException", "-", "Unused block", meios::diagnostic_code::xacro_structural_error},
        {"XacroException", "-", "Undefined parameters", meios::diagnostic_code::xacro_structural_error},
        {"XacroException", "-", "Xacro conditional", meios::diagnostic_code::xacro_structural_error},
        {"XacroException", "-", "Undefined block", meios::diagnostic_code::xacro_structural_error},
        {"XacroException", "-", "xacro:element: empty name", meios::diagnostic_code::xacro_structural_error},
        {"XacroException", "-", "xacro:attribute: empty name", meios::diagnostic_code::xacro_structural_error},
        {"XacroException", "XacroException", "invalid expression: ", meios::diagnostic_code::expression_error},
        {"XacroException", "XacroException", "circular variable definition", meios::diagnostic_code::xacro_structural_error},
        {"XacroException", "XacroException", "name '", meios::diagnostic_code::undefined_property},
        {"XacroException", "XacroException", "Cannot substitute $(dirname)", meios::diagnostic_code::unknown_substitution},
        {"XacroException", "ImportError", "substitution args not supported", meios::diagnostic_code::unknown_substitution},
        {"XacroException", "ArgException", "Undefined substitution argument", meios::diagnostic_code::unresolved_arg},
        {"XacroException", "NameError", "", meios::diagnostic_code::undefined_property},
        {"XacroException", "SyntaxError", "", meios::diagnostic_code::expression_error},
        {"XacroException", "ValueError", "", meios::diagnostic_code::expression_error},
        {"XacroException", "PackageNotFoundError", "", meios::diagnostic_code::unresolved_find},
        {"XacroException", "FileNotFoundError", "", meios::diagnostic_code::unresolved_include},
        {"XacroException", "NotADirectoryError", "", meios::diagnostic_code::unresolved_include},
        {"XacroException", "SubstitutionException", "Unknown substitution command", meios::diagnostic_code::unknown_substitution},
        {"XacroException", "SubstitutionException", "Cannot substitute $(dirname)", meios::diagnostic_code::unknown_substitution},
        {"XacroException", "SubstitutionException", "$(find pkg) accepts exactly one argument", meios::diagnostic_code::unresolved_find},
        {"XacroException", "SubstitutionException", "$(arg var)", meios::diagnostic_code::unresolved_arg},
        {"XacroException", "SubstitutionException", "$(env var)", meios::diagnostic_code::unresolved_env},
        {"XacroException", "SubstitutionException", "$(optenv var)", meios::diagnostic_code::unresolved_env},
        {"XacroException", "SubstitutionException", "environment variable", meios::diagnostic_code::unresolved_env},
        {"XacroException", "SubstitutionException", "$(eval ...) may not contain double underscore", meios::diagnostic_code::expression_error},
        {"XacroException", "SubstitutionException", "Dollar signs", meios::diagnostic_code::unterminated_substitution},
        {"XacroException", "SubstitutionException", "Invalid left parenthesis", meios::diagnostic_code::unterminated_substitution},
        {"RuntimeError", "-", "", meios::diagnostic_code::xacro_structural_error},
        {"IndexError", "-", "list index out of range", meios::diagnostic_code::xacro_structural_error},
};

// Upstream's substitution wrapper leads its message with the wrapped class's representation, which the inner
// class already carries, so the stem is matched after it.
inline std::string_view cause_text(std::string_view line)
{
    const std::size_t end = line.starts_with("<class '") ? line.find("'>: ") : std::string_view::npos;
    return end == std::string_view::npos ? line : line.substr(end + 4);
}

inline std::optional<meios::diagnostic_code> mapped_cause(std::span<const refusal_cause> rows, std::string_view outer, std::string_view inner, std::string_view line)
{
    const std::string_view text = cause_text(line);
    std::optional<refusal_cause> best;
    for(const refusal_cause &row : rows)
    {
        const bool applies = row.outer == outer && row.inner == inner && text.starts_with(row.stem);
        if(applies && (!best || row.stem.size() > best->stem.size()))
            best = row;
    }
    return best ? std::optional<meios::diagnostic_code>{best->code} : std::nullopt;
}

inline std::optional<meios::diagnostic_code> mapped_cause(std::string_view outer, std::string_view inner, std::string_view line)
{
    return mapped_cause(refusal_causes, outer, inner, line);
}

// A line without the class columns parses with an empty outer class, which no row maps.
inline upstream_refusal parse_refusal(std::string_view render)
{
    constexpr std::string_view verdict = "REFUSED\t";
    std::string_view rest              = render.starts_with(verdict) ? render.substr(verdict.size()) : std::string_view{};
    while(rest.ends_with('\n') || rest.ends_with('\r'))
        rest.remove_suffix(1);
    const std::size_t first  = rest.find('\t');
    const std::size_t second = first == std::string_view::npos ? first : rest.find('\t', first + 1);
    if(second == std::string_view::npos)
        return upstream_refusal{{}, {}, std::string(rest)};
    return upstream_refusal{std::string(rest.substr(0, first)), std::string(rest.substr(first + 1, second - first - 1)), std::string(rest.substr(second + 1))};
}

}

#endif
