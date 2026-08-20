#include "../corpus_record.h"

#include <catch2/catch_test_macros.hpp>

#include <cctype>
#include <string>
#include <vector>
#include <cstddef>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace
{

const std::string records =
    "/corpus/a.urdf||core|"
    "|/corpus/b.xacro|ur_type=ur5e name=ur|python|/corpus/roots"
    "|/corpus/c.xacro|prefix=arm_|native|/corpus/roots"
    "|/corpus/d.xacro||lua|/corpus/roots";

std::string listfile_text(const std::string &name)
{
    const std::filesystem::path path = std::filesystem::path{ MEIOS_CMAKE_MODULE_DIR } / name;
    INFO("listfile: " << path.string());
    REQUIRE(std::filesystem::exists(path));
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    const std::string text = buffer.str();
    REQUIRE_FALSE(text.empty());
    return text;
}

std::vector<std::string> call_arguments(const std::string &text, std::size_t from, std::size_t to)
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
std::vector<std::vector<std::string>> recorded_documents(const std::string &text)
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

TEST_CASE("a corpus record selects the evaluator its document needs", "[corpus]")
{
    const std::vector<corpus::document> docs = corpus::documents(records);
    REQUIRE(docs.size() == 4);
    CHECK(docs[0].backend == corpus::evaluator::core);
    CHECK(docs[1].backend == corpus::evaluator::python);
    CHECK(docs[2].backend == corpus::evaluator::native);
    CHECK(docs[3].backend == corpus::evaluator::unknown);
    CHECK(docs[3].spelling == "lua");
}

TEST_CASE("a corpus record carries its expansion arguments and package root", "[corpus]")
{
    const std::vector<corpus::document> docs = corpus::documents(records);
    REQUIRE(docs.size() == 4);
    CHECK(docs[0].path == "/corpus/a.urdf");
    CHECK(docs[0].args.empty());
    CHECK(docs[0].package_root.empty());
    REQUIRE(docs[1].args.size() == 2);
    CHECK(docs[1].args.at("ur_type") == "ur5e");
    CHECK(docs[1].args.at("name") == "ur");
    CHECK(docs[1].package_root == "/corpus/roots");
    CHECK(docs[2].args.at("prefix") == "arm_");
}

TEST_CASE("a record list that is not a whole number of records parses to nothing", "[corpus]")
{
    CHECK(corpus::documents("/corpus/a.urdf||core").empty());
    CHECK(corpus::documents("/corpus/a.urdf||core||/corpus/b.xacro||native").empty());
    CHECK(corpus::documents("").empty());
}

TEST_CASE("the pinned expression-valued description is recorded for the built-in evaluator",
          "[corpus]")
{
    const std::vector<std::vector<std::string>> found =
        recorded_documents(listfile_text("corpus_documents.cmake"));
    REQUIRE(found.size() >= 3);

    std::size_t pinned = 0;
    for(const std::vector<std::string> &record : found)
    {
        REQUIRE(record.size() == 4);
        if(record[0].find("ur.urdf.xacro") == std::string::npos)
            continue;
        INFO("recorded for " << record[1] << " as " << record[2]);
        CHECK(record[2] == "native");
        ++pinned;
    }
    CHECK(pinned == 5);
}

TEST_CASE("no corpus record asks for the optional evaluator", "[corpus]")
{
    const std::string text = listfile_text("corpus_documents.cmake");
    for(const std::vector<std::string> &record : recorded_documents(text))
    {
        REQUIRE(record.size() == 4);
        INFO("record for " << record[0] << " naming " << record[2]);
        CHECK(corpus::classify(record[2]) != corpus::evaluator::unknown);
        CHECK(corpus::classify(record[2]) != corpus::evaluator::python);
    }
}

TEST_CASE("the corpus no longer claims the built-in evaluator cannot subscript", "[corpus]")
{
    const std::string text = listfile_text("corpus_documents.cmake");
    CHECK(text.find("only the Python evaluator") == std::string::npos);
    CHECK(text.find("only the CPython evaluator") == std::string::npos);
    CHECK(text.find("eval-python") == std::string::npos);
}
