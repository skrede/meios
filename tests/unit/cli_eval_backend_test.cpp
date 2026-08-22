#include "verbs/verbs.h"

#include "meios/completion/command_table.h"

#include <catch2/catch_test_macros.hpp>

#include <cctype>
#include <string>
#include <fstream>
#include <ostream>
#include <sstream>
#include <utility>
#include <iostream>
#include <string_view>
#include <filesystem>

using namespace meios;
using meios::cli::verb_context;

namespace
{

class cout_capture
{
public:
    cout_capture() : m_buffer(), m_saved(std::cout.rdbuf(m_buffer.rdbuf())) {}

    ~cout_capture() { std::cout.rdbuf(m_saved); }

    cout_capture(const cout_capture &) = delete;
    cout_capture &operator=(const cout_capture &) = delete;

    std::string str() const { return m_buffer.str(); }

private:
    std::ostringstream m_buffer;
    std::streambuf *m_saved;
};

// The name attribute carries a python-only list literal the core evaluator declines:
// a fail policy aborts, lenient policies leave it verbatim, the python backend renders it.
constexpr std::string_view python_span_doc =
    "<robot name=\"probe_${['x']}\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
    "<link name=\"base_link\"/></robot>";

// The name attribute reaches the process's own authority: the restricted evaluator refuses
// it, and an unguarded one would interpolate the working directory into the emitted document.
constexpr std::string_view reaching_span_doc =
    "<robot name=\"probe_${__import__('os').getcwd()}\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
    "<link name=\"base_link\"/></robot>";

std::filesystem::path write_doc(const std::string &stem, std::string_view body)
{
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / ("meios_cli_eval_" + stem + ".xacro");
    std::ofstream(path) << body;
    return path;
}

std::filesystem::path write_probe(const std::string &stem) { return write_doc(stem, python_span_doc); }

std::string lowered(std::string_view text)
{
    std::string out(text);
    for(char &c : out)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

verb_context flatten_over(const std::filesystem::path &doc)
{
    verb_context ctx;
    ctx.id = "flatten";
    ctx.positionals = { doc.string() };
    return ctx;
}

}

TEST_CASE("cli_eval_backend: --eval-policy skip makes a python-only span non-fatal while the default fails")
{
    const std::filesystem::path doc = write_probe("policy");

    verb_context strict = flatten_over(doc);
    {
        cout_capture out;
        REQUIRE(cli::run_flatten(strict) != 0);
        REQUIRE(out.str().find("<robot") == std::string::npos);
    }

    verb_context lenient = flatten_over(doc);
    lenient.value_flags["--eval-policy"] = "skip";
    {
        cout_capture out;
        REQUIRE(cli::run_flatten(lenient) == 0);
        REQUIRE(out.str().find("<robot") != std::string::npos);
    }
    std::filesystem::remove(doc);
}

TEST_CASE("cli_eval_backend: --eval python resolves a python-only span when the backend is linked")
{
    if(!cli::eval_python_linked())
    {
        SUCCEED("meios::eval-python not linked; the python backend is absent");
        return;
    }
    const std::filesystem::path doc = write_probe("linked");
    verb_context ctx = flatten_over(doc);
    ctx.value_flags["--eval"] = "python";

    cout_capture out;
    const int code = cli::run_flatten(ctx);
    std::filesystem::remove(doc);

    REQUIRE(code == 0);
    REQUIRE(out.str().find("probe_['x']") != std::string::npos);
}

TEST_CASE("cli_eval_backend: --eval python binds the restricted evaluator and refuses a reaching span")
{
    if(!cli::eval_python_linked())
    {
        SUCCEED("meios::eval-python not linked; the python backend is absent");
        return;
    }
    const std::filesystem::path doc = write_doc("restricted", reaching_span_doc);
    verb_context ctx = flatten_over(doc);
    ctx.value_flags["--eval"] = "python";

    cout_capture out;
    const int code = cli::run_flatten(ctx);
    std::filesystem::remove(doc);

    REQUIRE(code != 0);
    REQUIRE(out.str().find("<robot") == std::string::npos);
}

TEST_CASE("cli_eval_backend: no spelling of an unrestricted opt-out reaches an unguarded evaluator")
{
    const std::filesystem::path doc = write_doc("optout", reaching_span_doc);
    const std::string cwd = std::filesystem::current_path().string();
    const std::pair<const char *, const char *> spellings[] = {
        { "--eval", "unrestricted" }, { "--eval", "python-unrestricted" }, { "--eval", "unrestricted-python" },
        { "--eval-unrestricted", "true" }, { "--unrestricted", "true" }, { "--eval-python-unrestricted", "python" },
    };
    for(const std::pair<const char *, const char *> &spelling : spellings)
    {
        INFO(spelling.first << '=' << spelling.second);
        verb_context ctx = flatten_over(doc);
        ctx.value_flags["--eval"]       = "python";
        ctx.value_flags[spelling.first] = spelling.second;

        cout_capture out;
        const int code = cli::run_flatten(ctx);

        REQUIRE(code != 0);
        REQUIRE(out.str().find("<robot") == std::string::npos);
        REQUIRE(out.str().find(cwd) == std::string::npos);
    }
    std::filesystem::remove(doc);
}

TEST_CASE("cli_eval_backend: the generated command surface names no unrestricted backend")
{
    for(const command_spec &command : cli_table())
    {
        INFO(command.name);
        REQUIRE(lowered(command.description).find("unrestricted") == std::string::npos);
        for(const flag_spec &flag : command.flags)
        {
            INFO(flag.token);
            REQUIRE(lowered(flag.token).find("unrestricted") == std::string::npos);
            REQUIRE(lowered(flag.description).find("unrestricted") == std::string::npos);
        }
    }
}

TEST_CASE("cli_eval_backend: --eval python fails loudly when the backend is not linked")
{
    if(cli::eval_python_linked())
    {
        SUCCEED("meios::eval-python linked; the absent-backend path is unreachable here");
        return;
    }
    const std::filesystem::path doc = write_probe("absent");
    verb_context ctx = flatten_over(doc);
    ctx.value_flags["--eval"] = "python";

    cout_capture out;
    const int code = cli::run_flatten(ctx);
    std::filesystem::remove(doc);

    REQUIRE(code != 0);
    REQUIRE(out.str().find("<robot") == std::string::npos);
}
