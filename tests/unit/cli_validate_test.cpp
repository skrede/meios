#include "verbs/verbs.h"
#include "verbs/validate.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <filesystem>

using namespace meios;
using meios::cli::verb_context;

namespace
{

std::string fixture(const std::string &name)
{
    return (std::filesystem::path(MEIOS_URDF_FIXTURE_DIR) / name).string();
}

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

int run_validate_on(const std::string &file, const std::string &format, std::string &printed)
{
    verb_context ctx;
    ctx.id = "validate";
    ctx.positionals = { file };
    if(!format.empty())
        ctx.value_flags["--format"] = format;
    cout_capture out;
    const int code = cli::run_validate(ctx);
    printed = out.str();
    return code;
}

}

TEST_CASE("cli_validate: a clean model exits zero")
{
    std::string printed;
    REQUIRE(run_validate_on(fixture("branched_all_joints.urdf"), "", printed) == 0);
    REQUIRE(printed.find("OK") != std::string::npos);
}

TEST_CASE("cli_validate: a malformed document is class 1")
{
    std::string printed;
    REQUIRE(run_validate_on(fixture("dup_attr.urdf"), "", printed) == 1);
    REQUIRE(run_validate_on(fixture("trailing_garbage.urdf"), "", printed) == 1);
    // A joint naming a link the document never declares is a property of the text rather
    // than of the graph, so it is refused before a record exists and reported as malformed.
    REQUIRE(run_validate_on(fixture("orphan_joint.urdf"), "", printed) == 1);
}

TEST_CASE("cli_validate: a connectivity failure is class 2")
{
    std::string printed;
    REQUIRE(run_validate_on(fixture("multi_root.urdf"), "", printed) == 2);
    REQUIRE(run_validate_on(fixture("cycle.urdf"), "", printed) == 2);
}

TEST_CASE("cli_validate: a missing bounded-joint limit is class 3")
{
    std::string printed;
    REQUIRE(run_validate_on(fixture("schema_missing_limit.urdf"), "", printed) == 3);
    REQUIRE(printed.find("schema") != std::string::npos);
}

TEST_CASE("cli_validate: an unsupported xacro feature is class 4")
{
    std::string printed;
    REQUIRE(run_validate_on(fixture("unsupported_xacro.xacro"), "", printed) == 4);
}

TEST_CASE("cli_validate: the most-severe class wins and the report lists the rest")
{
    std::string printed;
    const int code = run_validate_on(fixture("multi_class.urdf"), "", printed);
    REQUIRE(code == 2);
    REQUIRE(printed.find("[2]") != std::string::npos);
    REQUIRE(printed.find("[3]") != std::string::npos);
}

TEST_CASE("cli_validate: the exit code agrees with the reported status in both modes")
{
    std::string text;
    std::string json;
    const int text_code = run_validate_on(fixture("multi_class.urdf"), "", text);
    const int json_code = run_validate_on(fixture("multi_class.urdf"), "json", json);

    REQUIRE(text_code == json_code);
    REQUIRE(text.find("FAIL") != std::string::npos);
    REQUIRE(json.find("\"status\":\"fail\"") != std::string::npos);
    REQUIRE(json.find("\"exit_code\":2") != std::string::npos);
}

TEST_CASE("cli_validate: a clean model reports ok in json with a zero exit")
{
    std::string json;
    const int code = run_validate_on(fixture("branched_all_joints.urdf"), "json", json);
    REQUIRE(code == 0);
    REQUIRE(json.find("\"status\":\"ok\"") != std::string::npos);
    REQUIRE(json.find("\"exit_code\":0") != std::string::npos);
}
