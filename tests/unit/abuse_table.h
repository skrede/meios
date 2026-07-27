#ifndef HPP_GUARD_MEIOS_UNIT_ABUSE_TABLE_H
#define HPP_GUARD_MEIOS_UNIT_ABUSE_TABLE_H

#include <string>
#include <vector>
#include <cstddef>
#include <fstream>
#include <filesystem>
#include <string_view>

namespace abuse
{

constexpr std::string_view emitted_rules[] = { "dunder-identifier", "format-traversal",
                                               "non-allowlisted-builtin", "uncontained-yaml-path" };

struct row
{
    std::string expr;
    std::string verdict;
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
    fields.resize(4);
    return fields;
}

inline std::vector<row> load_rows()
{
    const std::filesystem::path path =
        std::filesystem::path{ MEIOS_GOLDEN_DIR } / "expr" / "abuse.cases";
    std::ifstream in(path);
    std::vector<row> rows;
    std::string line;
    while(std::getline(in, line))
    {
        if(line.empty() || line[0] == '#')
            continue;
        const std::vector<std::string> fields = fields_of(line);
        rows.push_back({ fields[0], fields[1], fields[2] });
    }
    return rows;
}

inline bool emitted(const std::string &rule)
{
    for(std::string_view known : emitted_rules)
    {
        if(rule == known)
            return true;
    }
    return false;
}

inline bool resolving_yaml(const row &r)
{
    return r.verdict == "allow" && r.expr.find("load_yaml") != std::string::npos;
}

}

#endif
