#include "xacro_arg_fixture.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <optional>
#include <algorithm>
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

std::string declaring_y(std::string_view link)
{
    return "<robot name=\"probe\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\"><xacro:arg name=\"y\" default=\"Y\"/>"
           "<link name=\"" + std::string(link) + "\"/></robot>";
}

void require_link_refused(std::string_view link, meios::diagnostic_code code)
{
    INFO("link " << link);
    recorder log;
    const expansion_result out = run(declaring_y(link), log);
    CHECK_FALSE(out.has_value());
    if(!out.has_value())
        CHECK(out.error().code == code);
}

struct kept_message
{
    std::optional<meios::diagnostic_code> code;
    std::string text;
};

struct transcript
{
    std::vector<kept_message> &kept;

    void operator()(meios::level, const std::string &text) { kept.push_back({ std::nullopt, text }); }

    void operator()(meios::level, const meios::source_location &, const std::string &text) { kept.push_back({ std::nullopt, text }); }

    void operator()(meios::level, meios::diagnostic_code code, const meios::source_location &, const std::string &text)
    {
        kept.push_back({ code, text });
    }
};

expansion_result run_under(std::string_view source, meios::eval_policy policy, std::vector<kept_message> &kept)
{
    meios::source_stack sources;
    meios::eval_scope scope;
    meios::log_sink_f sink{ transcript{ kept } };
    return meios::expand(source, scope, sources, "doc.xacro", meios::expansion_limits{}, policy, {}, sink);
}

bool logged(const std::vector<kept_message> &kept, meios::diagnostic_code code, std::string_view holds)
{
    return std::ranges::any_of(kept, [&](const kept_message &m) { return m.code == code && m.text.find(holds) != std::string::npos; });
}

void require_link_renders(std::string_view link, std::string_view name)
{
    INFO("link " << link);
    CHECK(flattened(declaring_y(link)) == meios::canonical_xml("<robot name=\"probe\"><link name=\"" + std::string(name) + "\"/></robot>"));
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

TEST_CASE("a command's words are separated by a space alone, so a tab stays inside a word, as the reference separates them", "[xacro][subst][cmd]")
{
    for(const char *link : { "$(arg&#9;y)", "$(&#9;arg y)", "$(eval&#9;1)", "l$(env&#9;HOME)" })
        require_link_refused(link, meios::diagnostic_code::unknown_substitution);
    require_link_refused("$(arg y&#9;)", meios::diagnostic_code::unresolved_arg);
    require_link_renders("l$(optenv MEIOS_UNSET_PROBE_NAME&#9;a)", "l");
    require_link_renders("$(arg  y)", "Y");
    require_link_renders("$( arg y)", "Y");
    require_link_renders("l$(eval 1&#9;)", "l1");
}

TEST_CASE("a refusal or a kept span inside a command is reported at its offset into the whole value", "[xacro][subst][cmd]")
{
    recorder log;
    const expansion_result refused = run(declaring_y("$(arg ${x)"), log);
    REQUIRE_FALSE(refused.has_value());
    CHECK(refused.error().message == "unterminated ${ substitution span at offset 6");
    std::vector<kept_message> kept;
    CHECK_FALSE(run_under(declaring_y("$(arg ${[1, 2]})"), meios::eval_policy::warn, kept).has_value());
    for(const kept_message &m : kept)
        UNSCOPED_INFO("logged: " << m.text);
    CHECK(logged(kept, meios::diagnostic_code::unsupported_expression, "verbatim at offset 6"));
}
