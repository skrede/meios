#include <meios/completion.h>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cctype>
#include <string>
#include <cstddef>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace
{

std::string slurp_golden(const std::string &name)
{
    const std::filesystem::path path =
        std::filesystem::path{ MEIOS_GOLDEN_DIR } / "completion" / name;
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

bool contains(const std::string &haystack, const std::string &needle)
{
    return haystack.find(needle) != std::string::npos;
}

// Reports a hit when the script invokes the shell eval builtin on dynamic output: a
// word-boundary "eval" (line start or preceded by whitespace) followed by one or more
// whitespace bytes and then a command-substitution, variable, string, or subshell opener.
// The run of whitespace is consumed wholesale, so no tab/newline/multi-space separator
// variation slips past — the guard tracks the invocation shape, not five literal forms.
bool eval_is_invoked(const std::string &script)
{
    for(std::size_t at = script.find("eval"); at != std::string::npos; at = script.find("eval", at + 1))
    {
        if(at != 0 && std::isspace(static_cast<unsigned char>(script[at - 1])) == 0)
            continue;
        std::size_t i = at + 4;
        const std::size_t gap = i;
        while(i < script.size() && std::isspace(static_cast<unsigned char>(script[i])) != 0)
            ++i;
        if(i == gap || i >= script.size())
            continue;
        const char target = script[i];
        if(target == '"' || target == '\'' || target == '$' || target == '`' || target == '(')
            return true;
    }
    return false;
}

meios::completion_model built_model()
{
    return meios::lower(meios::cli_table());
}

}

TEST_CASE("completion_emit: bash emitter matches its golden", "[completion]")
{
    REQUIRE(meios::emit_bash(built_model()) == slurp_golden("meios.bash"));
}

TEST_CASE("completion_emit: zsh emitter matches its golden", "[completion]")
{
    REQUIRE(meios::emit_zsh(built_model()) == slurp_golden("meios.zsh"));
}

TEST_CASE("completion_emit: fish emitter matches its golden", "[completion]")
{
    REQUIRE(meios::emit_fish(built_model()) == slurp_golden("meios.fish"));
}

TEST_CASE("completion_emit: every emitter carries descriptions from the table", "[completion]")
{
    const meios::completion_model model = built_model();
    const std::string phrase = "Expand xacro and resolve";
    REQUIRE(contains(meios::emit_bash(model), phrase));
    REQUIRE(contains(meios::emit_zsh(model), phrase));
    REQUIRE(contains(meios::emit_fish(model), phrase));
}

TEST_CASE("completion_emit: emitted scripts never eval candidate output", "[completion]")
{
    const meios::completion_model model = built_model();
    REQUIRE_FALSE(eval_is_invoked(meios::emit_bash(model)));
    REQUIRE_FALSE(eval_is_invoked(meios::emit_zsh(model)));
    REQUIRE_FALSE(eval_is_invoked(meios::emit_fish(model)));

    REQUIRE(eval_is_invoked("eval\t\"$candidate\""));
    REQUIRE(eval_is_invoked("eval   $candidate"));
    REQUIRE(eval_is_invoked("eval\n`candidate`"));

    REQUIRE_FALSE(eval_is_invoked("retrieval \"$x\""));
}

TEST_CASE("completion_emit: bash expansions are quoted, never bare", "[completion]")
{
    const std::string bash = meios::emit_bash(built_model());
    const std::array<std::string, 4> bare = { "$cur", "$words", "$candidate", "$verb" };
    for(const std::string &token : bare)
        REQUIRE_FALSE(contains(bash, token));
}
