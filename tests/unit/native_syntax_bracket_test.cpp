#include "syntax_probe.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

using syntax_probe::loads;
using syntax_probe::quoted_at;
using syntax_probe::load_under;

TEST_CASE("a closing bracket where an operand is owed refuses under every evaluation policy", "[native][syntax]")
{
    for(meios::eval_policy policy : {meios::eval_policy::fail, meios::eval_policy::warn, meios::eval_policy::skip})
    {
        for(const quoted_at &owed : {quoted_at{"${1 + )}", "')'"}, quoted_at{"${max(1 + )}", "')'"}, quoted_at{"${1 + ]}", "']'"}})
        {
            INFO("body: " << owed.text);
            const meios::expected<meios::expansion, meios::expansion_error> out = load_under(owed.text, policy);
            REQUIRE_FALSE(out.has_value());
            CHECK(out.error().loc.file.string() == "robot.xacro");
            CHECK(out.error().loc.line > 0);
            CHECK(out.error().loc.column > 0);
            CHECK(out.error().message.find(owed.lexeme) != std::string::npos);
        }
        CHECK(loads("${1 + 2}", policy));
    }
}

TEST_CASE("a bracket that closes an empty argument list or follows a separator is left to the "
          "parser",
          "[native][syntax]")
{
    const probe::outcome bare     = probe::evaluate("max()");
    const probe::outcome trailing = probe::evaluate("max(1, )");

    REQUIRE((bare.messages.size() == 1 && trailing.messages.size() == 1));
    CHECK(bare.messages.front().find("max() needs an argument") != std::string::npos);
    CHECK(trailing.messages.front().find("unsupported expression at ')'") != std::string::npos);
    CHECK(probe::evaluate("max(1, 2)").rendered == "2");
    CHECK(probe::evaluate("full['radius']").rendered == "0.06");
    CHECK(probe::evaluate("2 ** -1").rendered == "0.5");
}

TEST_CASE("a closing bracket where an operand is owed refuses whichever way the decision goes", "[native][syntax]")
{
    for(std::string_view owed : {"True and )", "False and )", "True or )", "False or )", "1 if True else )", "1 if False else )"})
    {
        INFO("expression: " << owed);
        const probe::outcome ran = probe::evaluate(owed);
        CHECK(ran.failed);
        CHECK(ran.kind == meios::eval_failure_kind::error);
        REQUIRE_FALSE(ran.messages.empty());
        CHECK(ran.messages.front().find("')'") != std::string::npos);
    }
    CHECK(probe::evaluate("False and (1)").kind == meios::eval_failure_kind::none);
    CHECK(probe::evaluate("1 if False else (2)").rendered == "2");
}
