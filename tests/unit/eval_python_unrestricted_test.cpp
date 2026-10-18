#include "eval_python_either_fixture.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <fstream>
#include <utility>
#include <optional>
#include <filesystem>

using namespace either_evaluator;

namespace
{

struct golden_case
{
    std::string expr;
    std::string expected;
};

std::vector<golden_case> load_cases(const std::string &name)
{
    const std::filesystem::path path = std::filesystem::path{MEIOS_GOLDEN_DIR} / "expr" / name;
    std::ifstream in(path);
    std::vector<golden_case> cases;
    std::string line;
    while(std::getline(in, line))
    {
        std::size_t tab = line.find('\t');
        if(line.empty() || line[0] == '#' || tab == std::string::npos)
            continue;
        cases.push_back({line.substr(0, tab), line.substr(tab + 1)});
    }
    return cases;
}

}

static_assert(meios::text_evaluator<meios::unrestricted_python_evaluator>);

// One expression per rule the guard enforces — dunder identifier, withheld builtin, formatting
// traversal — each refused by the restricted evaluator and evaluated by the unrestricted one.
TEST_CASE("every rule the restricted evaluator enforces is absent from the unrestricted one", "[eval_python]")
{
    const meios::eval_scope scope;
    const std::pair<const char *, std::string> paired[] = {
            {"__import__('os').getcwd()", std::filesystem::current_path().string()},
            {"getattr(1, 'real')", "1"},
            {"'{}'.format(2)", "2"},
    };
    for(const std::pair<const char *, std::string> &c : paired)
    {
        INFO(c.first);
        REQUIRE(refuse_of<meios::python_evaluator>(c.first, scope).refused);
        const std::optional<std::string> got = evaluate<meios::unrestricted_python_evaluator>(c.first, scope);
        REQUIRE(got);
        REQUIRE(*got == c.second);
    }
}

TEST_CASE("both evaluators render every golden corpus row identically", "[eval_python]")
{
    const meios::eval_scope scope;
    for(const char *file : {"arithmetic.cases", "functions.cases", "precedence.cases"})
    {
        for(const golden_case &c : load_cases(file))
        {
            INFO(file << ": " << c.expr);
            const std::optional<std::string> got = evaluate<meios::unrestricted_python_evaluator>(c.expr, scope);
            REQUIRE(got);
            REQUIRE(*got == c.expected);
            REQUIRE(got == evaluate<meios::python_evaluator>(c.expr, scope));
        }
    }
}
