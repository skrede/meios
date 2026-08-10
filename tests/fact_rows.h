#ifndef HPP_GUARD_MEIOS_TESTS_FACT_ROWS_H
#define HPP_GUARD_MEIOS_TESTS_FACT_ROWS_H

#include "unit/oracle_records.h"

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <string>
#include <vector>
#include <cstddef>
#include <cstdlib>
#include <optional>
#include <algorithm>

namespace facts
{

inline std::vector<std::string> tokens(const std::string &text)
{
    std::vector<std::string> out;
    for(std::size_t at = 0; at < text.size();)
    {
        const std::size_t space = std::min(text.find(' ', at), text.size());
        if(space > at)
            out.push_back(text.substr(at, space - at));
        at = space + 1;
    }
    return out;
}

// A recorded value either gives one number per key ("effort=9.0 velocity=6.28") or opens a key
// several numbers belong to ("xyz=0.0 0.0 0.138"), so a token carrying no '=' extends the key
// the last one opened. Numbers are compared as numbers rather than as text: the record holds the
// text upstream renders, which for a document that evaluates nothing is the author's own spelling
// and need not match any renderer's.
inline std::map<std::string, std::vector<double>> keyed_numbers(const std::string &text)
{
    std::map<std::string, std::vector<double>> out;
    std::string key;
    for(const std::string &token : tokens(text))
    {
        const std::size_t split_at = token.find('=');
        if(split_at == std::string::npos)
        {
            if(!key.empty())
                out[key].push_back(std::strtod(token.c_str(), nullptr));
            continue;
        }
        key = token.substr(0, split_at);
        out[key].push_back(std::strtod(token.c_str() + split_at + 1, nullptr));
    }
    return out;
}

inline std::string value_of(const std::vector<oracle::row> &rows, const std::string &key)
{
    INFO("record fact: " << key);
    const std::optional<oracle::row> found = oracle::lookup(rows, key);
    REQUIRE(found.has_value());
    REQUIRE(found->fields.size() > 1);
    return found->fields[1];
}

inline std::size_t rows_under(const std::vector<oracle::row> &rows, const std::string &prefix)
{
    std::size_t count = 0;
    for(const oracle::row &one : rows)
        if(!one.fields.empty() && one.fields.front().rfind(prefix, 0) == 0)
            ++count;
    return count;
}

}

#endif
