#ifndef HPP_GUARD_MEIOS_UNIT_COVERAGE_MATRIX_H
#define HPP_GUARD_MEIOS_UNIT_COVERAGE_MATRIX_H

#include "oracle_records.h"

#include <string>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <filesystem>
#include <string_view>

namespace coverage
{

// The surfaces this library answers for, as the requirement behind the committed matrix names
// them, with the resource ceilings as one member rather than one per axis: the axes are
// enumerated in their own artifact, and twenty cells stay readable where seventy would not.
enum class surface : std::uint32_t
{
    native_value_conversion,
    auxiliary_document_parsing,
    property_propagation,
    expression_parsing,
    resource_limits,
};

struct surface_spelling
{
    surface named;
    std::string_view name;
};

// Closed at these spellings with no default arm, the way the construct vocabulary is closed: a
// spelling outside the table resolves to nothing rather than reaching a member by accident.
inline constexpr surface_spelling surface_spellings[] = {
    { surface::native_value_conversion, "native-value-conversion" },
    { surface::auxiliary_document_parsing, "auxiliary-document-parsing" },
    { surface::property_propagation, "property-propagation" },
    { surface::expression_parsing, "expression-parsing" },
    { surface::resource_limits, "resource-limits" }
};

inline constexpr std::size_t surface_count = std::size(surface_spellings);

// Each row sits at the index of the member it names, which makes the pairing total.
constexpr bool surface_spellings_are_paired()
{
    for(std::size_t at = 0; at < surface_count; ++at)
        if(surface_spellings[at].named != static_cast<surface>(at))
            return false;
    return true;
}

static_assert(surface_spellings_are_paired(),
              "each spelling sits at the index of the surface it names");
static_assert(static_cast<std::size_t>(surface::resource_limits) + 1 == surface_count,
              "every surface carries a spelling");

// The four kinds the requirement names, and a fifth for lifetime and sanitizer evidence. The
// fifth reads the requirement wider than written and the committed matrix's own header declares
// it as such, with the evidence behind it; a widening absorbed silently is the same defect as a
// narrowing absorbed silently.
enum class kind : std::uint32_t
{
    negative,
    property,
    fuzz,
    concurrency,
    lifetime,
};

struct kind_spelling
{
    kind named;
    std::string_view name;
};

inline constexpr kind_spelling kind_spellings[] = { { kind::negative, "negative" },
                                                   { kind::property, "property" },
                                                   { kind::fuzz, "fuzz" },
                                                   { kind::concurrency, "concurrency" },
                                                   { kind::lifetime, "lifetime" } };

inline constexpr std::size_t kind_count = std::size(kind_spellings);

constexpr bool kind_spellings_are_paired()
{
    for(std::size_t at = 0; at < kind_count; ++at)
        if(kind_spellings[at].named != static_cast<kind>(at))
            return false;
    return true;
}

static_assert(kind_spellings_are_paired(), "each spelling sits at the index of the kind it names");
static_assert(static_cast<std::size_t>(kind::lifetime) + 1 == kind_count,
              "every kind carries a spelling");

inline std::string_view surface_name(surface one)
{
    return surface_spellings[static_cast<std::size_t>(one)].name;
}

inline std::optional<surface> surface_from_name(std::string_view spelling)
{
    for(const surface_spelling &one : surface_spellings)
    {
        if(one.name == spelling)
            return one.named;
    }
    return std::nullopt;
}

inline std::string_view kind_name(kind one)
{
    return kind_spellings[static_cast<std::size_t>(one)].name;
}

inline std::optional<kind> kind_from_name(std::string_view spelling)
{
    for(const kind_spelling &one : kind_spellings)
    {
        if(one.name == spelling)
            return one.named;
    }
    return std::nullopt;
}

// The artifacts are hand-authored rather than measurement output, so they live beside the record
// directory instead of inside it: the configure step globs that directory for the record
// extension and raises a fatal error for any file there carrying no digest, which would demand a
// measurement run for every edit made here. The directory is matrix rather than coverage because
// coverage/ is an ignore pattern for the profiler's output and would swallow these files.
inline std::vector<oracle::row> load_coverage(std::string_view file)
{
    const std::filesystem::path path = std::filesystem::path{ MEIOS_GOLDEN_DIR } / "matrix" / file;
    std::ifstream in(path);
    if(!in)
        throw std::runtime_error("the coverage artifact " + path.string() + " did not open");
    const std::vector<oracle::row> rows = oracle::rows_of(in);
    if(rows.empty())
        throw std::runtime_error("the coverage artifact " + path.string() + " carries no rows");
    return rows;
}

}

#endif
