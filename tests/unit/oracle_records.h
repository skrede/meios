#ifndef HPP_GUARD_MEIOS_UNIT_ORACLE_RECORDS_H
#define HPP_GUARD_MEIOS_UNIT_ORACLE_RECORDS_H

#include <string>
#include <vector>
#include <cstddef>
#include <fstream>
#include <istream>
#include <sstream>
#include <optional>
#include <filesystem>
#include <stdexcept>
#include <string_view>

namespace oracle
{

struct row
{
    std::vector<std::string> fields;
};

inline std::vector<std::string> fields_of(const std::string &line)
{
    std::vector<std::string> fields;
    std::size_t start = 0;
    for(std::size_t tab = line.find('\t'); tab != std::string::npos; tab = line.find('\t', start))
    {
        fields.push_back(line.substr(start, tab - start));
        start = tab + 1;
    }
    fields.push_back(line.substr(start));
    return fields;
}

inline std::vector<row> rows_of(std::istream &in)
{
    std::vector<row> rows;
    std::string line;
    while(std::getline(in, line))
    {
        if(line.empty() || line[0] == '#')
            continue;
        rows.push_back(row{ fields_of(line) });
    }
    return rows;
}

// A record that does not open, or opens empty, would otherwise become zero rows that every
// downstream loop reads as nothing to assert. The header serves an out-of-tree program as well as
// the test binaries, so the refusal is an exception rather than a test-framework macro.
inline std::vector<row> load_rows(std::string_view file)
{
    const std::filesystem::path path =
        std::filesystem::path{ MEIOS_GOLDEN_DIR } / "oracle" / file;
    std::ifstream in(path);
    if(!in)
        throw std::runtime_error("the record " + path.string() + " did not open");
    const std::vector<row> rows = rows_of(in);
    if(rows.empty())
        throw std::runtime_error("the record " + path.string() + " carries no rows");
    return rows;
}

// A recorded failure is upstream's own wording, so an independent implementation can only be held
// to naming the same thing upstream named, not to repeating the sentence.
inline bool relates(const std::vector<std::string> &messages, const std::string &recorded)
{
    std::istringstream words(recorded);
    for(std::string word; words >> word;)
        for(const std::string &message : messages)
            if(word.size() >= 4 && message.find(word) != std::string::npos)
                return true;
    return false;
}

inline std::optional<row> lookup(const std::vector<row> &rows, std::string_view key)
{
    for(const row &one : rows)
    {
        if(!one.fields.empty() && one.fields.front() == key)
            return one;
    }
    return std::nullopt;
}

}

#endif
