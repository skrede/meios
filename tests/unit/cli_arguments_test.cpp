#include "app.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <filesystem>

using namespace meios;

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

struct flattened
{
    int code;
    std::string document;
};

flattened flatten_with(std::vector<std::string> overrides)
{
    std::vector<std::string> args = {"meios", "flatten", (std::filesystem::path(MEIOS_URDF_FIXTURE_DIR) / "caller_arguments_stripped.xacro").string()};
    args.insert(args.end(), overrides.begin(), overrides.end());
    std::vector<char *> argv;
    for(std::string &arg : args)
        argv.push_back(arg.data());
    cout_capture out;
    const int code = cli::run(static_cast<int>(argv.size()), argv.data());
    return flattened{code, out.str()};
}

}

TEST_CASE("cli_arguments: an override is bound with the white space around its value removed, separator characters among it")
{
    const flattened ran = flatten_with({"a:= x ", "b:=\x1c" "1"});
    REQUIRE(ran.code == 0);
    CHECK(ran.document.find("\"l_x_2\"") != std::string::npos);
}

TEST_CASE("cli_arguments: an override is bound under its name with the white space around it removed")
{
    const flattened ran = flatten_with({" a :=y"});
    REQUIRE(ran.code == 0);
    CHECK(ran.document.find("\"l_y_1\"") != std::string::npos);
}

TEST_CASE("cli_arguments: an override whose name is only white space refuses as an empty key and prints no document")
{
    const flattened ran = flatten_with({" :=y"});
    CHECK(ran.code != 0);
    CHECK(ran.document.find("<robot") == std::string::npos);
}
