#ifndef HPP_GUARD_MEIOS_TESTS_GENERATED_RECORD_H
#define HPP_GUARD_MEIOS_TESTS_GENERATED_RECORD_H

#include "../unit/oracle_records.h"

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <string>
#include <vector>
#include <cstddef>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <string_view>

namespace generated
{

struct family
{
    std::size_t count;
    std::vector<std::string> slots;
    std::string layout;
};

struct generated_case
{
    std::string id;
    std::string family;
    std::string text;
    std::string upstream;
};

struct record
{
    std::map<std::string, family> families;
    std::vector<std::string> escaped;
    std::vector<generated_case> cases;
};

inline std::filesystem::path oracle_file(std::string_view name)
{
    return std::filesystem::path{MEIOS_GOLDEN_DIR} / "oracle" / name;
}

inline std::vector<std::string> words_of(const std::string &text)
{
    std::istringstream in(text);
    std::vector<std::string> words;
    for(std::string word; in >> word;)
        words.push_back(word);
    return words;
}

inline std::string unescaped(std::string_view column)
{
    std::string out;
    for(std::size_t at = 0; at < column.size(); ++at)
    {
        if(column[at] != '\\' || at + 1 == column.size())
        {
            out += column[at];
            continue;
        }
        const char next = column[++at];
        out += next == 't' ? '\t' : next == 'n' ? '\n' : next;
    }
    return out;
}

inline std::string attribute_escaped(std::string_view text)
{
    std::string out;
    for(const char c : text)
        out += c == '&' ? std::string("&amp;") : c == '<' ? std::string("&lt;") : c == '"' ? std::string("&quot;") : std::string(1, c);
    return out;
}

inline void read_header_line(record &held, const std::string &line)
{
    const std::size_t space = line.find(' ');
    const std::string key   = line.substr(0, space);
    const std::string rest  = space == std::string::npos ? std::string() : line.substr(space + 1);
    const std::size_t name  = rest.find(' ');
    if(key == "escaped")
        held.escaped = words_of(rest);
    else if(key == "count")
        held.families[rest.substr(0, name)].count = std::stoul(rest.substr(name + 1));
    else if(key == "slots")
        held.families[rest.substr(0, name)].slots = words_of(rest.substr(name + 1));
    else if(key == "template")
        held.families[rest.substr(0, name)].layout = rest.substr(name + 1);
}

inline record read_record()
{
    std::ifstream in(oracle_file("generated_cases.cases"));
    REQUIRE(in);
    record held;
    for(std::string line; std::getline(in, line);)
    {
        if(line.rfind("# ", 0) == 0)
            read_header_line(held, line.substr(2));
        else if(!line.empty())
        {
            const std::vector<std::string> fields = oracle::fields_of(line);
            REQUIRE(fields.size() == 5);
            held.cases.push_back(generated_case{fields[0], fields[1], unescaped(fields[2]), fields[3]});
        }
    }
    return held;
}

inline std::vector<std::string> slot_values(const std::string &text)
{
    std::vector<std::string> values{std::string()};
    for(const char c : text)
    {
        if(c == '\t')
            values.emplace_back();
        else
            values.back() += c;
    }
    return values;
}

inline std::string slot_value(const record &held, const family &layout, const std::vector<std::string> &values, const std::string &slot)
{
    const auto at = std::ranges::find(layout.slots, slot);
    REQUIRE(at != layout.slots.end());
    const std::string &value = values.at(static_cast<std::size_t>(at - layout.slots.begin()));
    return std::ranges::find(held.escaped, slot) != held.escaped.end() ? attribute_escaped(value) : value;
}

// Every document is built from the record's own header, so the comparator holds no template of its
// own that could drift from what the reference was handed.
inline std::string document_of(const record &held, const generated_case &one)
{
    const family &layout                  = held.families.at(one.family);
    const std::vector<std::string> values = slot_values(one.text);
    REQUIRE(values.size() == layout.slots.size());
    std::string out;
    std::size_t at = 0;
    for(std::size_t open = layout.layout.find('@'); open != std::string::npos; open = layout.layout.find('@', at))
    {
        const std::size_t close = layout.layout.find('@', open + 1);
        REQUIRE(close != std::string::npos);
        out += layout.layout.substr(at, open - at) + slot_value(held, layout, values, layout.layout.substr(open + 1, close - open - 1));
        at = close + 1;
    }
    return out + layout.layout.substr(at);
}

inline std::vector<oracle::row> reviewed_rows()
{
    std::ifstream in(oracle_file("generated_divergences.cases"));
    REQUIRE(in);
    return oracle::rows_of(in);
}

}

#endif
