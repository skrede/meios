#include <meios/xacro.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <string_view>

#ifndef MEIOS_GOLDEN_DIR
    #error "MEIOS_GOLDEN_DIR must name the golden-corpus root"
#endif

namespace
{

struct expr_case
{
    std::string expression;
    std::string expected;
    int line;
};

std::string trim(std::string_view text)
{
    std::size_t first = text.find_first_not_of(" \t\r\n");
    if(first == std::string_view::npos)
        return std::string();
    std::size_t last = text.find_last_not_of(" \t\r\n");
    return std::string(text.substr(first, last - first + 1));
}

std::vector<expr_case> read_cases(const std::filesystem::path &file)
{
    std::vector<expr_case> cases;
    std::ifstream input(file);
    std::string line;
    for(int number = 1; std::getline(input, line); ++number)
    {
        std::string stripped = trim(line);
        if(stripped.empty() || stripped.front() == '#')
            continue;
        std::size_t delimiter = line.find(line.find('\t') != std::string::npos ? '\t' : '|');
        REQUIRE(delimiter != std::string::npos);
        cases.push_back({ trim(line.substr(0, delimiter)), trim(line.substr(delimiter + 1)), number });
    }
    return cases;
}

std::string eval_str(const std::string &expression)
{
    meios::core_evaluator evaluator;
    meios::eval_scope scope;
    meios::log_sink sink;
    return meios::to_python_str(evaluator.eval(expression, scope, sink));
}

}

TEST_CASE("every frozen golden expression matches the core evaluator", "[xacro][corpus]")
{
    const std::filesystem::path root = std::filesystem::path(MEIOS_GOLDEN_DIR) / "expr";
    int total = 0;
    for(const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(root))
    {
        if(entry.path().extension() != ".cases")
            continue;
        for(const expr_case &row : read_cases(entry.path()))
        {
            INFO(entry.path().filename().string() << ':' << row.line << "  " << row.expression);
            REQUIRE(eval_str(row.expression) == row.expected);
            ++total;
        }
    }
    REQUIRE(total > 0);
}
