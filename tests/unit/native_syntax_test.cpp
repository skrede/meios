#include "branch_probe.h"

#include <meios/io/source_stack.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <functional>
#include <string_view>

namespace
{
// A refusal abandons whatever the prefix had produced, so the null spelling standing where a
// number or a boolean stood is what distinguishes a refusal from a value that reached the
// document.
bool refused(const probe::outcome &ran)
{
    return ran.failed && ran.rendered == "None";
}

meios::expected<meios::expansion, meios::expansion_error> load_under(std::string_view body,
                                                                     meios::eval_policy policy)
{
    const std::string document = std::string("<robot xmlns:xacro=\"http://ros.org/wiki/xacro\">")
                               + "<l>" + std::string(body) + "</l></robot>";
    probe::recorder heard;
    meios::log_sink_f sink{ std::ref(heard) };
    meios::eval_scope scope;
    meios::source_stack sources;
    return meios::expand(document, scope, sources, "robot.xacro", meios::expansion_limits{},
                         policy, {}, sink);
}

bool loads(std::string_view body, meios::eval_policy policy)
{
    return load_under(body, policy).has_value();
}

struct quoted_at
{
    std::string_view text;
    std::string_view lexeme;
};
}

TEST_CASE("a malformed tail refuses whichever way the condition goes", "[native][syntax]")
{
    const probe::outcome taken = probe::evaluate("1 if True else 2 3");

    CHECK(refused(taken));
    CHECK(refused(probe::evaluate("1 if False else 2 3")));
    REQUIRE_FALSE(taken.messages.empty());
    CHECK(taken.messages.front().find("'3'") != std::string::npos);
}

TEST_CASE("a malformed tail refuses whichever way a boolean operand decides", "[native][syntax]")
{
    CHECK(refused(probe::evaluate("True or 1 3")));
    CHECK(refused(probe::evaluate("False or 1 3")));
    CHECK(refused(probe::evaluate("False and 1 3")));
    CHECK(refused(probe::evaluate("True and 1 3")));
}

TEST_CASE("a malformed conditional refuses inside a call argument", "[native][syntax]")
{
    CHECK(refused(probe::evaluate("max(1 if True else 2 3, 5)")));
}

TEST_CASE("an adjacency the parser already refused still refuses", "[native][syntax]")
{
    CHECK(refused(probe::evaluate("1 + 2 3")));
    CHECK(refused(probe::evaluate("(1 if True else 2) 3")));
}

TEST_CASE("an operator standing where an operand belongs refuses", "[native][syntax]")
{
    const probe::outcome doubled = probe::evaluate("1 if True else 2 * * 2");

    CHECK(refused(doubled));
    REQUIRE_FALSE(doubled.messages.empty());
    CHECK(doubled.messages.front().find("'*'") != std::string::npos);
    CHECK(refused(probe::evaluate("(1+ if False else 2")));
    CHECK(refused(probe::evaluate("1 2 if False else 3")));
}

TEST_CASE("an expression ending where an operand belongs refuses", "[native][syntax]")
{
    const probe::outcome dangling = probe::evaluate("1 if True else 2 +");

    CHECK(refused(dangling));
    REQUIRE_FALSE(dangling.messages.empty());
    CHECK(dangling.messages.front().find("ends where an operand is expected") != std::string::npos);
    CHECK(refused(probe::evaluate("False and 1 +")));
    CHECK(refused(probe::evaluate("1 if True else")));
    CHECK(refused(probe::evaluate("False and (")));
    CHECK(refused(probe::evaluate("False and")));
    CHECK(refused(probe::evaluate("False and 1 2")));
}

TEST_CASE("an adjacency the parser names better is left to the parser", "[native][syntax]")
{
    const probe::outcome no_else = probe::evaluate("1 if True");
    const probe::outcome no_first = probe::evaluate("if True else 1");
    const probe::outcome empty = probe::evaluate("");
    const probe::outcome text = probe::evaluate("word and True");

    REQUIRE_FALSE(no_else.messages.empty());
    CHECK(no_else.messages.front().find("expected 'else' in conditional") != std::string::npos);
    REQUIRE_FALSE(no_first.messages.empty());
    CHECK(no_first.messages.front().find("'if'") != std::string::npos);
    CHECK_FALSE(empty.messages.empty());
    CHECK(text.kind == meios::eval_failure_kind::unsupported);
    REQUIRE(text.messages.size() == 1);
    CHECK(text.messages.front().find("no arithmetic meaning") != std::string::npos);
}

TEST_CASE("a well-formed expression still evaluates", "[native][syntax]")
{
    CHECK(probe::evaluate("1 if True else 2").rendered == "1");
    CHECK(probe::evaluate("1 if False else 2").rendered == "2");
    CHECK(probe::evaluate("xacro.load_yaml(bare_file)['length']").rendered == "0.12");
    CHECK(probe::evaluate("full['radius' if True else 'length']").rendered == "0.06");
    CHECK(probe::evaluate("max(1, 2 if False else 5)").rendered == "5");
    CHECK(probe::evaluate("2 ** -1").rendered == "0.5");
    CHECK(probe::evaluate("not not True").rendered == "True");
    CHECK(probe::evaluate("-(4-1)").rendered == "-3");
    CHECK(probe::evaluate("'radius' in cfg").rendered == "False");
}

TEST_CASE("a syntax refusal is terminal under every evaluation policy", "[native][syntax]")
{
    for(meios::eval_policy policy :
        { meios::eval_policy::fail, meios::eval_policy::warn, meios::eval_policy::skip })
    {
        CHECK_FALSE(loads("${1 if True else 2 3}", policy));
        CHECK(loads("${1 if True else 2}", policy));
    }
}

TEST_CASE("a closing bracket where an operand is owed refuses under every evaluation policy",
          "[native][syntax]")
{
    for(meios::eval_policy policy :
        { meios::eval_policy::fail, meios::eval_policy::warn, meios::eval_policy::skip })
    {
        for(const quoted_at &owed : { quoted_at{ "${1 + )}", "')'" },
                                      quoted_at{ "${max(1 + )}", "')'" },
                                      quoted_at{ "${1 + ]}", "']'" } })
        {
            INFO("body: " << owed.text);
            const meios::expected<meios::expansion, meios::expansion_error> out =
                load_under(owed.text, policy);
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
          "parser", "[native][syntax]")
{
    const probe::outcome bare = probe::evaluate("max()");
    const probe::outcome trailing = probe::evaluate("max(1, )");

    REQUIRE((bare.messages.size() == 1 && trailing.messages.size() == 1));
    CHECK(bare.messages.front().find("max() needs an argument") != std::string::npos);
    CHECK(trailing.messages.front().find("unsupported expression at ')'") != std::string::npos);
    CHECK(probe::evaluate("max(1, 2)").rendered == "2");
    CHECK(probe::evaluate("full['radius']").rendered == "0.06");
    CHECK(probe::evaluate("2 ** -1").rendered == "0.5");
}

// This evaluator reads neither a mapping literal nor a sequence literal, so the scan leaves both
// to the parser, and only the branch actually taken is parsed -- so their verdict is still
// decided at run time. Settling it in the scan would mean re-encoding a grammar the parser owns.
TEST_CASE("a construct the parser owns keeps its own verdict whichever way the decision goes",
          "[native][syntax]")
{
    CHECK(probe::evaluate("1 if True else {1: 2}").rendered == "1");
    CHECK(probe::evaluate("1 if True else [3]").rendered == "1");
    CHECK(probe::evaluate("True or {1: 2}").rendered == "True");
    // A refused disjunction still carries its left operand, so the failure flag rather than the
    // null spelling is what marks a refusal here.
    CHECK(probe::evaluate("False or {1: 2}").rendered == "False");
    for(const quoted_at &owned : { quoted_at{ "1 if False else {1: 2}", "'{'" },
                                   quoted_at{ "1 if False else [3]", "'['" },
                                   quoted_at{ "False or {1: 2}", "'{'" } })
    {
        INFO("expression: " << owned.text);
        const probe::outcome ran = probe::evaluate(owned.text);
        CHECK(ran.failed);
        REQUIRE_FALSE(ran.messages.empty());
        CHECK(ran.messages.front().find("unsupported expression at " + std::string(owned.lexeme))
              != std::string::npos);
    }
}
