#ifndef HPP_GUARD_MEIOS_UNIT_CONTAINER_TABLE_H
#define HPP_GUARD_MEIOS_UNIT_CONTAINER_TABLE_H

#include <string>
#include <vector>
#include <cstddef>
#include <fstream>
#include <filesystem>
#include <string_view>

namespace container
{

constexpr std::string_view verdicts[] = { "accept", "refuse" };

struct row
{
    std::string subject;
    std::string expr;
    std::string verdict;
    std::string expect;
    std::string note;
};

inline int hex_digit(char c)
{
    if(c >= '0' && c <= '9')
        return c - '0';
    if(c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}

inline int hex_byte(const std::string &text, std::size_t at)
{
    if(at + 3 >= text.size() || text[at + 1] != 'x')
        return -1;
    const int high = hex_digit(text[at + 2]);
    const int low  = hex_digit(text[at + 3]);
    return high < 0 || low < 0 ? -1 : high * 16 + low;
}

// The data file carries no other escape, and no field may contain a tab or a newline; a
// backslash sequence that is neither \xNN nor \\ is left exactly as it was written.
inline std::string unescape(const std::string &text)
{
    std::string out;
    for(std::size_t at = 0; at < text.size(); ++at)
    {
        const int byte = text[at] == '\\' ? hex_byte(text, at) : -1;
        if(byte >= 0)
        {
            out.push_back(static_cast<char>(byte));
            at += 3;
        }
        else if(text[at] == '\\' && at + 1 < text.size() && text[at + 1] == '\\')
            out.push_back(text[at++]);
        else
            out.push_back(text[at]);
    }
    return out;
}

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
        std::filesystem::path{ MEIOS_GOLDEN_DIR } / "expr" / "container.cases";
    std::ifstream in(path);
    std::vector<row> rows;
    std::string line;
    while(std::getline(in, line))
    {
        if(line.empty() || line[0] == '#')
            continue;
        const std::vector<std::string> fields = fields_of(line);
        rows.push_back({ unescape(fields[0]), fields[1], fields[2], fields[3], fields[4] });
    }
    return rows;
}

inline bool known(const std::string &verdict)
{
    for(std::string_view spelling : verdicts)
    {
        if(verdict == spelling)
            return true;
    }
    return false;
}

}

#endif
