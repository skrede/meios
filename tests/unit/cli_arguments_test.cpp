#include "app.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <ostream>
#include <sstream>
#include <utility>
#include <iostream>
#include <filesystem>

using namespace meios;

namespace
{

class stream_capture
{
public:
    explicit stream_capture(std::ostream &stream) : m_stream(stream), m_buffer(), m_saved(stream.rdbuf(m_buffer.rdbuf())) {}

    ~stream_capture() { m_stream.rdbuf(m_saved); }

    stream_capture(const stream_capture &) = delete;
    stream_capture &operator=(const stream_capture &) = delete;

    std::string str() const { return m_buffer.str(); }

private:
    std::ostream &m_stream;
    std::ostringstream m_buffer;
    std::streambuf *m_saved;
};

struct flattened
{
    int code;
    std::string document;
    std::string errors;
};

flattened flatten_with(std::vector<std::string> overrides)
{
    std::vector<std::string> args = {"meios", "flatten", (std::filesystem::path(MEIOS_URDF_FIXTURE_DIR) / "caller_arguments_stripped.xacro").string()};
    args.insert(args.end(), overrides.begin(), overrides.end());
    std::vector<char *> argv;
    for(std::string &arg : args)
        argv.push_back(arg.data());
    stream_capture out(std::cout);
    stream_capture err(std::cerr);
    const int code = cli::run(static_cast<int>(argv.size()), argv.data());
    return flattened{code, out.str(), err.str()};
}

std::size_t warnings_in(const std::string &errors)
{
    std::size_t count = 0;
    std::istringstream lines(errors);
    for(std::string line; std::getline(lines, line);)
        count += line.find("[warn]") != std::string::npos ? 1U : 0U;
    return count;
}

bool renders(const flattened &ran, const std::string &link)
{
    return ran.document.find("\"" + link + "\"") != std::string::npos;
}

void require_without_effect(const std::string &token)
{
    INFO("token: '" << token << "'");
    const flattened ran = flatten_with({token});
    REQUIRE(ran.code == 0);
    CHECK(renders(ran, "l_d_1"));
    CHECK(renders(ran, "p_d_d_d"));
    CHECK(warnings_in(ran.errors) == 1);
    CHECK(ran.errors.find("'" + token + "'") != std::string::npos);
}

}

TEST_CASE("cli_arguments: an override is bound with the white space around its value removed, separator characters among it")
{
    const flattened ran = flatten_with({"a:= x ", "b:=\x1c" "1"});
    REQUIRE(ran.code == 0);
    CHECK(renders(ran, "l_x_2"));
    CHECK(warnings_in(ran.errors) == 0);
}

TEST_CASE("cli_arguments: an override is bound under its name with the white space around it removed")
{
    const flattened ran = flatten_with({" a := x "});
    REQUIRE(ran.code == 0);
    CHECK(renders(ran, "l_x_1"));
    CHECK(warnings_in(ran.errors) == 0);
}

TEST_CASE("cli_arguments: an override whose name is empty once stripped is dropped with a warning and the document renders its defaults")
{
    for(const std::string token : {":=1", " :=1"})
        require_without_effect(token);
}

TEST_CASE("cli_arguments: an override whose value is empty once stripped is dropped with a warning and the argument keeps its default")
{
    for(const std::string token : {"a:=", "a:= "})
        require_without_effect(token);
}

TEST_CASE("cli_arguments: an override whose name has one leading underscore is ignored with a warning")
{
    require_without_effect("_p:=1");
}

TEST_CASE("cli_arguments: a name of two leading underscores or a lone underscore binds without a warning")
{
    const std::vector<std::pair<std::string, std::string>> bound = {{"__p:=1", "p_d_1_d"}, {"_:=1", "p_d_d_1"}};
    for(const auto &[token, link] : bound)
    {
        INFO("token: '" << token << "'");
        const flattened ran = flatten_with({token});
        REQUIRE(ran.code == 0);
        CHECK(renders(ran, link));
        CHECK(warnings_in(ran.errors) == 0);
    }
}

TEST_CASE("cli_arguments: each override without effect writes its own warning and the bound ones write none")
{
    const flattened ran = flatten_with({"a:=", "__p:=1", ":=1", "b:=1", "_p:=1"});
    REQUIRE(ran.code == 0);
    CHECK(renders(ran, "l_d_2"));
    CHECK(renders(ran, "p_d_1_d"));
    CHECK(warnings_in(ran.errors) == 3);
}

TEST_CASE("cli_arguments: a token without an assignment refuses as a malformed override and prints no document")
{
    const flattened ran = flatten_with({"a"});
    CHECK(ran.code != 0);
    CHECK(ran.document.find("<robot") == std::string::npos);
    CHECK(ran.errors.find("malformed argument override 'a'") != std::string::npos);
}
