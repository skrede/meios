#include <meios/completion.h>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <string>
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
    const std::array<std::string, 3> scripts = {
        meios::emit_bash(model), meios::emit_zsh(model), meios::emit_fish(model)
    };
    for(const std::string &script : scripts)
        REQUIRE_FALSE(contains(script, "eval"));
}

TEST_CASE("completion_emit: bash expansions are quoted, never bare", "[completion]")
{
    const std::string bash = meios::emit_bash(built_model());
    const std::array<std::string, 4> bare = { "$cur", "$words", "$candidate", "$verb" };
    for(const std::string &token : bare)
        REQUIRE_FALSE(contains(bash, token));
}
