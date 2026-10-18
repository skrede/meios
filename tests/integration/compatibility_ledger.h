#ifndef HPP_GUARD_MEIOS_TESTS_COMPATIBILITY_LEDGER_H
#define HPP_GUARD_MEIOS_TESTS_COMPATIBILITY_LEDGER_H

#include "../unit/oracle_records.h"

#include <meios/xacro.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <optional>
#include <algorithm>

namespace ledger
{

enum column
{
    entry_point,
    document_path,
    revision,
    arguments,
    upstream_result,
    native_result,
    exercised_constructs,
    reviewed_divergence,
    column_count
};

inline std::vector<oracle::row> rows()
{
    return oracle::load_rows("compatibility_ledger.cases");
}

inline std::vector<std::string> spellings_in(const std::string &cell)
{
    std::vector<std::string> found;
    for(std::size_t at = 0; at < cell.size();)
    {
        const std::size_t stop = std::min(cell.find(',', at), cell.size());
        found.push_back(cell.substr(at, stop - at));
        at = cell.find_first_not_of(' ', stop + 1);
        if(at == std::string::npos)
            break;
    }
    return found;
}

// The vocabulary is what a spelling is read against, and it answers an unrecognized one with
// nothing, so a column cannot drift into prose without failing here.
inline meios::construct_set set_of(const std::vector<std::string> &spellings)
{
    meios::construct_set out;
    for(const std::string &one : spellings)
    {
        INFO("construct spelling: " << one);
        const std::optional<meios::evaluator_construct> found = meios::construct_from_name(one);
        REQUIRE(found.has_value());
        out.mark(*found);
    }
    return out;
}

}

#endif
