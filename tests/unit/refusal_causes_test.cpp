#include "oracle_records.h"

#include "../refusal_causes.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>
#include <cstddef>
#include <optional>
#include <algorithm>
#include <string_view>

namespace
{

using meios::diagnostic_code;

// A site's own message is matched by a row of its class, where an empty stem stands only for a site that fixes
// no text. A site upstream wraps is matched by a row naming its class as the inner one.
bool covers(const differential::refusal_cause &row, std::string_view type, std::string_view fixed)
{
    if(row.inner == type)
        return row.stem.empty() || fixed.empty() || fixed.starts_with(row.stem);
    return row.outer == type && (row.stem.empty() ? fixed.empty() : fixed.starts_with(row.stem));
}

// A wrapper fixes no text of its own and passes on whatever it caught, so it is covered by the rows
// of the classes it is measured to wrap, never by any row that happens to share its class.
constexpr std::pair<std::string_view, std::string_view> wrapped_classes[]{
        {"raise.xacroexception_eval_extension", "NameError"}, {"raise.xacroexception_eval_extension", "SubstitutionException"},
        {"raise.xacroexception_handle_expr", "NameError"},    {"raise.xacroexception_handle_expr", "XacroException"},
        {"raise.xacroexception_parse", "FileNotFoundError"},  {"raise.xacroexception_parse", "NotADirectoryError"},
};

bool wrapper_covered(std::string_view id)
{
    bool listed = false;
    for(const auto &[site, inner] : wrapped_classes)
    {
        if(site != id)
            continue;
        listed = true;
        if(std::ranges::none_of(differential::refusal_causes, [&](const differential::refusal_cause &row) { return row.outer == "XacroException" && row.inner == inner; }))
            return false;
    }
    return listed;
}

// The text a site fixes ends at its first format placeholder.
bool mapped_site(const oracle::row &site)
{
    const std::string_view quote = site.fields[4];
    const std::size_t colon      = quote.find(": ");
    const std::string_view type  = quote.substr(0, colon);
    const std::string_view rest  = colon == std::string_view::npos ? std::string_view{} : quote.substr(colon + 2);
    const std::string_view fixed = rest.substr(0, rest.find_first_of("%{"));
    if(type == "XacroException" && fixed.empty())
        return wrapper_covered(site.fields[0]);
    for(const differential::refusal_cause &row : differential::refusal_causes)
        if(covers(row, type, fixed))
            return true;
    return false;
}

}

TEST_CASE("an upstream cause maps to the code given here for it", "[refusal_causes]")
{
    REQUIRE(differential::mapped_cause("XacroException", "NameError", "name 'w' is not defined") == diagnostic_code::undefined_property);
    REQUIRE(differential::mapped_cause("XacroException", "-", "circular variable definition: a -> b -> a") == diagnostic_code::xacro_structural_error);
    REQUIRE(differential::mapped_cause("NoSuchError", "-", "x") == std::nullopt);
}

TEST_CASE("the longest matching stem decides, and an empty stem matches any line", "[refusal_causes]")
{
    constexpr differential::refusal_cause rows[]{
            {"XacroException", "-", "", diagnostic_code::xacro_structural_error},
            {"XacroException", "-", "Undefined substitution argument", diagnostic_code::unresolved_arg},
    };
    REQUIRE(differential::mapped_cause(rows, "XacroException", "-", "Undefined substitution argument a") == diagnostic_code::unresolved_arg);
    REQUIRE(differential::mapped_cause(rows, "XacroException", "-", "anything else") == diagnostic_code::xacro_structural_error);
    REQUIRE(differential::mapped_cause(rows, "XacroException", "NameError", "anything else") == std::nullopt);
}

TEST_CASE("a substitution failure is matched after the wrapped class's representation", "[refusal_causes]")
{
    const std::string_view line = "<class 'xacro.substitution_args.SubstitutionException'>: Unknown substitution command [x]";
    REQUIRE(differential::mapped_cause("XacroException", "SubstitutionException", line) == diagnostic_code::unknown_substitution);
}

TEST_CASE("a refusal render parses into its class, inner class and first line", "[refusal_causes]")
{
    const differential::upstream_refusal cause = differential::parse_refusal("REFUSED\tXacroException\tNameError\tname 'w' is not defined\n");
    REQUIRE(cause.outer == "XacroException");
    REQUIRE(cause.inner == "NameError");
    REQUIRE(cause.line == "name 'w' is not defined");
    REQUIRE(differential::parse_refusal("REFUSED\tXacroException\t-\t[find\tp]").line == "[find\tp]");
}

TEST_CASE("a refusal render without the class columns maps to nothing", "[refusal_causes]")
{
    const differential::upstream_refusal legacy = differential::parse_refusal("REFUSED\tname 'w' is not defined");
    REQUIRE(legacy.outer.empty());
    REQUIRE(differential::mapped_cause(legacy.outer, legacy.inner, legacy.line) == std::nullopt);
}

TEST_CASE("every raise site of the grammar maps to a diagnostic code", "[refusal_causes]")
{
    std::string unmapped;
    for(const oracle::row &site : oracle::load_rows("grammar_constructs.cases"))
    {
        REQUIRE(site.fields.size() == 5);
        if(site.fields[1] == "raise" && site.fields[2] == "grammar" && !mapped_site(site))
            unmapped += "\n  " + site.fields[0] + " (" + site.fields[3] + "): " + site.fields[4];
    }
    if(!unmapped.empty())
        FAIL("no refusal cause maps these raise sites:" << unmapped);
}
