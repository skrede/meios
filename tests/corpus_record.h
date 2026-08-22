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
// The records are named in cmake/corpus_documents.cmake; nothing here globs. A field count
// that is not a whole number of records is not partially parsed — it yields nothing.
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

// The join record_for() keys by, exposed on its own so a caller needing the id a document keys
// under -- the live differential's inventory and manifest rows are keyed this way, not by the
// record filename record_for() resolves to -- has one source for both.
// std::map already iterates in key order, so joining every doc.args entry needs no separate sort;
// a document sharing one argument value with another therefore keys distinctly, not aliased.
inline std::string document_key(const document &doc)
{
    std::string key;
    for(const std::pair<const std::string, std::string> &arg : doc.args)
        key += (key.empty() ? "" : " ") + arg.first + "=" + arg.second;
    return key.empty() ? doc.path.filename().string() : key;
}

// Every document the built-in evaluator is asked to handle carries measured upstream facts, so a
// document added without them fails here rather than passing on a load that merely exited. Each
// pairing is named outright, on the rule the corpus file names its documents by: a record name
// composed from a document's own arguments would make an unmeasured variant look covered.
inline std::string record_for(const document &doc)
{
    const std::map<std::string, std::string> measured{
        { "ur_type=ur5e", "ur5e_facts.cases" },
        { "ur_type=ur3e", "ur3e_facts.cases" },
        { "ur_type=ur7e", "ur7e_facts.cases" },
        { "safety_limits=true ur_type=ur5e", "ur5e_safety_facts.cases" },
        { "force_abs_paths=true ur_type=ur5e", "ur5e_abs_paths_facts.cases" },
        { "kr6r900sixx.xacro", "kr6_facts.cases" },
        { "lbr_med14_r820.urdf.xacro", "lbr_med14_r820_facts.cases" },
        { "fer.urdf.xacro", "fer_facts.cases" },
        { "fp3.urdf.xacro", "fp3_facts.cases" },
        { "fr3.urdf.xacro", "fr3_facts.cases" },
        { "fr3v2.urdf.xacro", "fr3v2_facts.cases" },
        { "fr3v2_1.urdf.xacro", "fr3v2_1_facts.cases" },
        { "tmrv0_2.urdf.xacro", "tmrv0_2_facts.cases" },
        { "dof=7", "gen3_facts.cases" },
        { "name=ur_mocked ur_type=ur5e", "ur_mocked_facts.cases" },
        { "gantry.urdf.xacro", "gantry_facts.cases" }
    };
    const std::map<std::string, std::string>::const_iterator found = measured.find(document_key(doc));
    return found == measured.end() ? std::string{} : found->second;
}

}

#endif
