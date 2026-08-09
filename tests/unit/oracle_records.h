#ifndef HPP_GUARD_MEIOS_UNIT_ORACLE_RECORDS_H
#define HPP_GUARD_MEIOS_UNIT_ORACLE_RECORDS_H

#include <string>
#include <vector>
#include <cstddef>
#include <fstream>
#include <optional>
#include <filesystem>
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

inline std::vector<row> load_rows(std::string_view file)
{
    const std::filesystem::path path =
        std::filesystem::path{ MEIOS_GOLDEN_DIR } / "oracle" / file;
    std::ifstream in(path);
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
