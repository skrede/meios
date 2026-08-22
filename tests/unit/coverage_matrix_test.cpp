#include "coverage_sources.h"
#include "coverage_matrix.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <algorithm>
#include <filesystem>
#include <string_view>

namespace
{

constexpr std::size_t evaluator_axis_count = 11;
constexpr std::size_t expansion_axis_count = 3;
constexpr std::size_t axis_column_count = 5;
constexpr std::string_view markers[] = { "tests:", "advisory:", "reason:" };
constexpr std::string_view reaches[] = { "crossed", "charged", "unreached" };
constexpr std::string_view aggregates[] = { "evaluator", "expansion" };
constexpr std::string_view empty_reasons[] = { "not applicable", "none", "n/a" };

std::vector<std::string> every_declared_axis()
{
    std::vector<std::string> all = coverage::evaluator_axes();
    REQUIRE(all.size() == evaluator_axis_count);
    const std::vector<std::string> expansion = coverage::expansion_axes();
    REQUIRE(expansion.size() == expansion_axis_count);
    all.insert(all.end(), expansion.begin(), expansion.end());
    return all;
}

void resolves(const std::string &word)
{
    INFO("reference: " << word);
    const coverage::reference cited = coverage::cited_by(word);
    REQUIRE(std::filesystem::exists(coverage::in_repository(cited.path)));
    if(cited.line > 0)
        CHECK(coverage::lines_in(coverage::in_repository(cited.path)) >= cited.line);
    if(coverage::needs_registration(cited.path))
        CHECK(coverage::registered(coverage::stem_of(cited.path)));
}

void every_reference_in(const std::vector<oracle::row> &rows)
{
    for(const oracle::row &one : rows)
        for(const std::string &cell : one.fields)
            for(const std::string &word : coverage::words_of(cell))
                if(coverage::is_reference(word))
                    resolves(word);
}

std::vector<oracle::row> matrix()
{
    return coverage::load_coverage("surface_matrix.tsv");
}

std::vector<oracle::row> axes()
{
    return coverage::load_coverage("limit_axes.tsv");
}

}

// Without this an emptied artifact or an emptied vocabulary satisfies every loop below and the
// pairing reports total coverage of nothing, which is the failure this file exists to prevent.
TEST_CASE("the vocabularies and the artifacts they describe are all non-empty", "[coverage]")
{
    REQUIRE(coverage::surface_count > 0);
    REQUIRE(coverage::kind_count > 0);
    REQUIRE(matrix().size() == coverage::surface_count + 1);
    REQUIRE_FALSE(axes().empty());
    REQUIRE(every_declared_axis().size() == evaluator_axis_count + expansion_axis_count);
}

TEST_CASE("every surface has a row and every row names a surface", "[coverage]")
{
    const std::vector<oracle::row> rows = matrix();
    for(const coverage::surface_spelling &one : coverage::surface_spellings)
    {
        INFO("surface: " << one.name);
        CHECK(oracle::lookup(rows, one.name).has_value());
    }
    for(std::size_t at = 1; at < rows.size(); ++at)
    {
        INFO("row: " << rows[at].fields.front());
        CHECK(coverage::surface_from_name(rows[at].fields.front()).has_value());
    }
    CHECK(rows.size() - 1 == coverage::surface_count);
}

TEST_CASE("every kind has a column and every column names a kind", "[coverage]")
{
    const std::vector<std::string> named = matrix().front().fields;
    REQUIRE(named.size() == coverage::kind_count + 1);
    for(std::size_t at = 0; at < coverage::kind_count; ++at)
    {
        INFO("column " << at + 1 << ": " << named[at + 1]);
        CHECK(coverage::kind_from_name(named[at + 1]) == static_cast<coverage::kind>(at));
    }
    for(const coverage::kind_spelling &one : coverage::kind_spellings)
    {
        INFO("kind: " << one.name);
        CHECK(std::ranges::find(named, one.name) != named.end());
    }
}

// An empty cell claims nothing and is the exact ambiguity the matrix removes, and a cell reading
// only "not applicable" is an empty cell with more characters in it.
TEST_CASE("every cell says which kind of evidence it is, and says something", "[coverage]")
{
    const std::vector<oracle::row> rows = matrix();
    for(std::size_t at = 1; at < rows.size(); ++at)
    {
        const std::vector<std::string> cells = rows[at].fields;
        REQUIRE(cells.size() == coverage::kind_count + 1);
        for(std::size_t column = 1; column < cells.size(); ++column)
        {
            const coverage::kind named = static_cast<coverage::kind>(column - 1);
            INFO(cells.front() << " x " << coverage::kind_name(named) << ": " << cells[column]);
            const std::vector<std::string> words = coverage::words_of(cells[column]);
            REQUIRE_FALSE(words.empty());
            CHECK(std::ranges::find(markers, words.front()) != std::ranges::end(markers));
            CHECK(words.size() > 8);
            const std::string rest = cells[column].substr(words.front().size() + 1);
            CHECK(std::ranges::find(empty_reasons, rest) == std::ranges::end(empty_reasons));
        }
    }
}

// A cell citing a test that does not exist, or one no listfile registers, is a blank cell wearing
// a name, so a reference is resolved rather than accepted for being spelled.
TEST_CASE("every reference either artifact cites resolves and runs", "[coverage]")
{
    every_reference_in(matrix());
    every_reference_in(axes());
}

TEST_CASE("a cell citing only a job that cannot fail the build says so", "[coverage]")
{
    for(const oracle::row &one : matrix())
    {
        for(const std::string &cell : one.fields)
        {
            if(!cell.starts_with("advisory:"))
                continue;
            INFO("cell: " << cell);
            CHECK(cell.find("cannot fail the build") != std::string::npos);
        }
    }
}

TEST_CASE("every axis both limit aggregates declare has a row, and every row an axis", "[coverage]")
{
    const std::vector<std::string> declared = every_declared_axis();
    const std::vector<oracle::row> rows = axes();
    std::vector<std::string> carried;
    for(std::size_t at = 1; at < rows.size(); ++at)
    {
        const std::vector<std::string> row = rows[at].fields;
        REQUIRE(row.size() == axis_column_count);
        INFO("axis row: " << row[0] << " " << row[1]);
        CHECK(std::ranges::find(aggregates, row[0]) != std::ranges::end(aggregates));
        CHECK(std::ranges::find(declared, row[1]) != declared.end());
        CHECK(std::ranges::find(reaches, row[3]) != std::ranges::end(reaches));
        CHECK(coverage::words_of(row[4]).size() > 8);
        carried.push_back(row[1]);
    }
    for(const std::string &axis : declared)
    {
        INFO("declared axis: " << axis);
        CHECK(std::ranges::find(carried, axis) != carried.end());
    }
    CHECK(carried.size() == declared.size());
}

TEST_CASE("a spelling outside either table resolves to nothing", "[coverage]")
{
    CHECK_FALSE(coverage::surface_from_name("yaml-parsing").has_value());
    CHECK_FALSE(coverage::surface_from_name("").has_value());
    CHECK_FALSE(coverage::kind_from_name("regression").has_value());
    for(const coverage::surface_spelling &one : coverage::surface_spellings)
        CHECK(coverage::surface_from_name(one.name) == one.named);
    for(const coverage::kind_spelling &one : coverage::kind_spellings)
        CHECK(coverage::kind_from_name(one.name) == one.named);
}
