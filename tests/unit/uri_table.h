#ifndef HPP_GUARD_MEIOS_UNIT_URI_TABLE_H
#define HPP_GUARD_MEIOS_UNIT_URI_TABLE_H

#include <string>
#include <vector>
#include <cstddef>
#include <fstream>
#include <filesystem>
#include <string_view>

namespace uri
{

constexpr std::string_view verdicts[] = { "refuse", "drop", "accept" };

constexpr std::string_view no_code = "-";

struct row
{
    std::string fixture;
    std::string verdict;
    std::string code;
    std::string rule;
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
    fields.resize(5);
    return fields;
}

inline std::vector<row> load_rows()
{
    const std::filesystem::path path =
        std::filesystem::path{ MEIOS_GOLDEN_DIR } / "urdf" / "uri.cases";
    std::ifstream in(path);
    std::vector<row> rows;
    std::string line;
    while(std::getline(in, line))
    {
        if(line.empty() || line[0] == '#')
            continue;
        const std::vector<std::string> fields = fields_of(line);
        rows.push_back({ fields[0], fields[1], fields[2], fields[3] });
    }
    return rows;
}

inline bool declared_verdict(const std::string &verdict)
{
    for(std::string_view known : verdicts)
    {
        if(verdict == known)
            return true;
    }
    return false;
}

inline bool texture_row(const row &r)
{
    return r.fixture.starts_with("texture_");
}

}

#endif
