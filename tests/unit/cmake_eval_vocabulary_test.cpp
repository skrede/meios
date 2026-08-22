#include "verbs/verbs.h"

#include "meios/completion/command_table.h"

#include <catch2/catch_test_macros.hpp>

#include <set>
#include <cctype>
#include <string>
#include <fstream>
#include <utility>
#include <filesystem>
#include <string_view>

#if defined(MEIOS_CLI_BINARY) && !defined(_WIN32)
#include <cstdio>
#include <sys/wait.h>
#endif

using namespace meios;

namespace
{

constexpr std::string_view backends_assignment =
    "set_property(GLOBAL PROPERTY MEIOS_FLATTEN_EVAL_BACKENDS";

struct vocabulary
{
    int assignments;
    std::set<std::string> names;
};

std::set<std::string> words(std::string_view text)
{
    std::set<std::string> out;
    std::string word;
    for(char c : text)
    {
        if(std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_' || c == '-')
            word += c;
        else if(!word.empty())
            out.insert(std::exchange(word, std::string{}));
    }
    if(!word.empty())
        out.insert(word);
    return out;
}

void collect_assignment(const std::string &line, vocabulary &found)
{
    const std::string::size_type at = line.find(backends_assignment);
    if(at == std::string::npos)
        return;
    ++found.assignments;
    std::string rest = line.substr(at + backends_assignment.size());
    const std::string::size_type close = rest.find(')');
    if(close != std::string::npos)
        rest.resize(close);
    for(const std::string &name : words(rest))
        found.names.insert(name);
}

vocabulary module_vocabulary()
{
    vocabulary found{ 0, {} };
    const std::filesystem::path modules{ MEIOS_CMAKE_MODULE_DIR };
    for(const std::filesystem::directory_entry &entry :
        std::filesystem::directory_iterator{ modules })
    {
        if(entry.path().extension() != ".cmake")
            continue;
        std::ifstream file{ entry.path() };
        std::string line;
        while(std::getline(file, line))
            collect_assignment(line, found);
    }
    return found;
}

const flag_spec *eval_flag(std::string_view command_name)
{
    for(const command_spec &command : cli_table())
    {
        if(command.name != command_name)
            continue;
        for(const flag_spec &flag : command.flags)
            if(flag.token == "--eval")
                return &flag;
    }
    return nullptr;
}

// A flag description enumerates its values after a colon, in prose — "core or python.",
// "fail, warn, or skip." — so the connectives are the only words in that tail that are not
// values. This is a textual comparison because the flag carries no machine-readable list.
std::set<std::string> described_backends(const std::string &description)
{
    const std::string::size_type colon = description.rfind(':');
    if(colon == std::string::npos)
        return {};
    std::set<std::string> named = words(description.substr(colon + 1));
    named.erase("or");
    named.erase("and");
    return named;
}

#if defined(MEIOS_CLI_BINARY) && !defined(_WIN32)
std::filesystem::path fixture()
{
    return std::filesystem::path{ MEIOS_URDF_FIXTURE_DIR } / "test-desc.urdf";
}

struct shell_result
{
    int status;
    std::string output;
};

shell_result run_cli(const std::string &command)
{
    const std::string piped = command + " 2>&1";
    std::string output;
    FILE *pipe = popen(piped.c_str(), "r");
    if(pipe == nullptr)
        return { -1, {} };
    char buffer[4096];
    while(std::fgets(buffer, sizeof(buffer), pipe) != nullptr)
        output += buffer;
    const int closed = pclose(pipe);
    return { WIFEXITED(closed) ? WEXITSTATUS(closed) : -1, std::move(output) };
}
#endif

}

TEST_CASE("cmake_eval_vocabulary: the build module names its backends in one greppable assignment")
{
    const vocabulary module = module_vocabulary();
    REQUIRE(module.assignments == 1);
    REQUIRE_FALSE(module.names.empty());
}

TEST_CASE("cmake_eval_vocabulary: the build module's backends are the ones the flatten flag describes")
{
    const vocabulary module = module_vocabulary();
    const flag_spec *flag = eval_flag("flatten");
    REQUIRE(flag != nullptr);

    const std::set<std::string> described = described_backends(flag->description);
    REQUIRE_FALSE(module.names.empty());
    REQUIRE_FALSE(described.empty());
    REQUIRE(module.names == described);
}

TEST_CASE("cmake_eval_vocabulary: every backend the build module offers flattens through the binary")
{
#if defined(MEIOS_CLI_BINARY) && !defined(_WIN32)
    const vocabulary module = module_vocabulary();
    REQUIRE_FALSE(module.names.empty());
    for(const std::string &backend : module.names)
    {
        INFO(backend);
        if(backend == "python" && !cli::eval_python_linked())
        {
            SUCCEED("the python evaluator is not built into this binary");
            continue;
        }
        const shell_result out = run_cli(std::string{ MEIOS_CLI_BINARY } + " flatten "
                                         + fixture().string() + " --eval " + backend);
        REQUIRE(out.status == 0);
        REQUIRE(out.output.find("<robot") != std::string::npos);
    }
#else
    SUCCEED("CLI binary or POSIX shell unavailable — the binary half skipped");
#endif
}
