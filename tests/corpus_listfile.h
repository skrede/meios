#ifndef HPP_GUARD_MEIOS_TESTS_CORPUS_LISTFILE_H
#define HPP_GUARD_MEIOS_TESTS_CORPUS_LISTFILE_H

#include <cctype>
#include <string>
#include <vector>
#include <cstddef>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <stdexcept>

namespace corpus
{

// A listfile that does not open, or opens empty, would otherwise become zero records that every
// downstream loop reads as nothing to assert, so the refusal is an exception rather than a
// test-framework macro -- the same rule the record reader states.
inline std::string listfile_text(const std::string &name)
{
    const std::filesystem::path path = std::filesystem::path{ MEIOS_CMAKE_MODULE_DIR } / name;
    std::ifstream in(path, std::ios::binary);
    if(!in)
        throw std::runtime_error("the listfile " + path.string() + " did not open");
    std::ostringstream buffer;
    buffer << in.rdbuf();
    const std::string text = buffer.str();
    if(text.empty())
        throw std::runtime_error("the listfile " + path.string() + " is empty");
    return text;
}

inline std::vector<std::string> call_arguments(const std::string &text, std::size_t from,
                                               std::size_t to)
{
    std::vector<std::string> args;
    for(std::size_t at = from; at < to;)
    {
        if(std::isspace(static_cast<unsigned char>(text[at])) != 0)
        {
            ++at;
            continue;
        }
        const bool quoted = text[at] == '"';
        const std::size_t start = quoted ? at + 1 : at;
        const std::size_t stop =
            quoted ? text.find('"', start) : text.find_first_of(" \t\r\n", start);
        args.push_back(text.substr(start, std::min(stop, to) - start));
        at = std::min(stop, to) + 1;
    }
    return args;
}

// The macro's own definition line spells the name without an attached parenthesis, so scanning for
// the call shape reaches every record and nothing else.
inline std::vector<std::vector<std::string>> recorded_documents(const std::string &text)
{
    const std::string call = "meios_corpus_document(";
    std::vector<std::vector<std::string>> found;
    for(std::size_t at = text.find(call); at != std::string::npos; at = text.find(call, at + 1))
    {
        const std::size_t open = at + call.size();
        found.push_back(call_arguments(text, open, text.find(')', open)));
    }
    return found;
}

}

#endif
