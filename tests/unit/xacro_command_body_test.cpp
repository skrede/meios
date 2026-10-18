#include "xacro_arg_fixture.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <string_view>

namespace
{

std::string probe(std::string_view fixture)
{
    std::ifstream in(std::filesystem::path{MEIOS_XACRO_PROBE_DIR} / (std::string(fixture) + ".xacro"), std::ios::binary);
    REQUIRE(in);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

void require_unterminated(std::string_view fixture)
{
    INFO("probe " << fixture);
    recorder log;
    const expansion_result out = run(probe(fixture), log);
    REQUIRE_FALSE(out.has_value());
    CHECK(out.error().code == meios::diagnostic_code::unterminated_substitution);
}

}

TEST_CASE("a command written inside a command refuses, as the reference's pattern ends the outer one at its first closing parenthesis", "[xacro][subst][cmd]")
{
    require_unterminated("command_nested_in_default");
    require_unterminated("command_nested_beside_bound_argument");
    require_unterminated("command_nested_in_find");
}

TEST_CASE("a parenthesis or a dollar sign left in a command's text refuses, as the reference's command scanner refuses it", "[xacro][subst][cmd]")
{
    require_unterminated("command_parenthesis_in_body");
    require_unterminated("command_dollar_in_body");
    require_unterminated("command_expanded_parenthesis");
}

TEST_CASE("an argument named through an expression span resolves as the reference resolves it", "[xacro][subst][cmd]")
{
    REQUIRE(flattened(probe("command_expanded_argument_name")) == meios::canonical_xml(R"XML(<robot name="probe"><link name="A"/></robot>)XML"));
}
