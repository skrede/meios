#ifndef HPP_GUARD_MEIOS_TESTS_CORPUS_RECORD_H
#define HPP_GUARD_MEIOS_TESTS_CORPUS_RECORD_H

#include <map>
#include <string>
#include <vector>
#include <cstddef>
#include <algorithm>
#include <filesystem>
#include <string_view>

namespace corpus
{

enum class evaluator
{
    core,
    python,
    native,
    unknown
};

struct document
{
    std::filesystem::path path;
    std::map<std::string, std::string> args;
    std::filesystem::path package_root;
    evaluator backend;
    std::string spelling;
};

inline evaluator classify(std::string_view field)
{
    if(field == "core")
        return evaluator::core;
    if(field == "python")
        return evaluator::python;
    if(field == "native")
        return evaluator::native;
    return evaluator::unknown;
}

inline std::vector<std::string> split(const std::string &text, char sep)
{
    std::vector<std::string> fields;
    for(std::size_t start = 0; start <= text.size();)
    {
        const std::size_t at = std::min(text.find(sep, start), text.size());
        fields.push_back(text.substr(start, at - start));
        start = at + 1;
    }
    return fields;
}

inline std::map<std::string, std::string> arg_map(const std::string &spec)
{
    std::map<std::string, std::string> args;
    for(const std::string &pair : split(spec, ' '))
    {
        const std::size_t split_at = pair.find('=');
        if(split_at != std::string::npos)
            args.emplace(pair.substr(0, split_at), pair.substr(split_at + 1));
    }
    return args;
}

// Four fields per record, joined with '|': path, expansion arguments, evaluator, package root.
// The records are named in cmake/corpus.cmake; nothing here globs. A field count that is not a
// whole number of records is not partially parsed — it yields nothing.
inline std::vector<document> documents(const std::string &records)
{
    const std::vector<std::string> fields = split(records, '|');
    std::vector<document> docs;
    if(fields.size() % 4 != 0)
        return docs;
    for(std::size_t at = 0; at + 3 < fields.size(); at += 4)
        docs.push_back({ fields[at], arg_map(fields[at + 1]), fields[at + 3],
                         classify(fields[at + 2]), fields[at + 2] });
    return docs;
}

}

#endif
