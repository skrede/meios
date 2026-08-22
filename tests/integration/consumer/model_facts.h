#ifndef HPP_GUARD_CONSUMER_MODEL_FACTS_H
#define HPP_GUARD_CONSUMER_MODEL_FACTS_H

// The committed record is the one authority on what a correct model is, and it is read here
// through the very loader the in-tree case reads it with. A copy of either inside this program
// would be a second definition of a correct model, free to drift from the first.
#include "unit/oracle_records.h"

#include <meios/model.h>

#include <map>
#include <string>
#include <vector>
#include <cstddef>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <optional>

namespace consumer
{

int check_structure(const meios::model<double> &robot, const std::vector<oracle::row> &rows);
int check_limits(const meios::model<double> &robot, const std::vector<oracle::row> &rows);
int check_assets(const meios::model<double> &robot, const std::vector<oracle::row> &rows);

// Every probe owes the record all three groups, so the triple is spelled once here rather than
// per probe, where one could quietly go missing.
inline int check_all_facts(const meios::model<double> &robot,
                           const std::vector<oracle::row> &rows)
{
    if(const int rc = check_structure(robot, rows))
        return rc;
    if(const int rc = check_limits(robot, rows))
        return rc;
    return check_assets(robot, rows);
}

// Exactly, not within a tolerance. The record holds the decimal text upstream renders, and
// round-tripping that text through strtod reproduces the same double, so two correct sides
// compare equal bit for bit; admitting a tolerance here would make this program's definition of a
// correct model looser than the one the record already serves.
inline bool same_number(double recorded, double loaded) { return recorded == loaded; }

inline std::string text_of(double value)
{
    std::ostringstream out;
    out << std::setprecision(17) << value;
    return out.str();
}

inline int refuse(const std::string &what)
{
    std::cerr << what << '\n';
    return 1;
}

inline int refuse_fact(const std::string &key, const std::string &recorded,
                       const std::string &loaded)
{
    std::cerr << "fact " << key << ": recorded '" << recorded << "', loaded '" << loaded << "'\n";
    return 1;
}

inline int refuse_absent(const std::string &key)
{
    std::cerr << "fact " << key << ": the record carries no such row, so nothing was compared\n";
    return 1;
}

inline int check_text(const std::vector<oracle::row> &rows, const std::string &key,
                      const std::string &loaded)
{
    const std::optional<oracle::row> found = oracle::lookup(rows, key);
    if(!found || found->fields.size() < 2)
        return refuse_absent(key);
    return found->fields[1] == loaded ? 0 : refuse_fact(key, found->fields[1], loaded);
}

// A recorded value either gives one number per key ("effort=9.0 velocity=6.28") or opens a key
// several numbers belong to ("xyz=0.0 0.0 0.138"), so a token carrying no '=' extends the key the
// last one opened.
inline std::map<std::string, std::vector<double>> keyed_numbers(const std::string &text)
{
    std::map<std::string, std::vector<double>> out;
    std::istringstream in(text);
    std::string token;
    std::string key;
    while(in >> token)
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
