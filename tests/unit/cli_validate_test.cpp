#include "verbs/verbs.h"
#include "verbs/validate.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstddef>
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

std::size_t count_of(const std::string &text, const std::string &needle)
{
    std::size_t found = 0;
    for(std::string::size_type at = text.find(needle); at != std::string::npos;
        at = text.find(needle, at + needle.size()))
        ++found;
    return found;
}

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

// The library refuses a bounded joint that declares no limit, so the rule is reported by the
// load rather than by a second command-line pass, and it is reported exactly once.
TEST_CASE("cli_validate: a missing bounded-joint limit is reported once, by the library")
{
    std::string printed;
    REQUIRE(run_validate_on(fixture("profile/joint_limit_absent.urdf"), "", printed) == 1);
    REQUIRE(printed.find("malformed") != std::string::npos);
    REQUIRE(count_of(printed, "<limit>") == 1);
}

TEST_CASE("cli_validate: an unsupported xacro feature is class 4")
{
    std::string printed;
    REQUIRE(run_validate_on(fixture("unsupported_xacro.xacro"), "", printed) == 4);
}

// The rule the classes obey is a property of the report rather than of any one document, and
// no document produces two classes now that the library owns every rule the verb once had.
TEST_CASE("cli_validate: the most-severe class wins and the report lists the rest")
{
    cli::validation_report report;
    report.findings.push_back(cli::validation_finding{ 4, "unsupported-feature", { "a" } });
    report.findings.push_back(cli::validation_finding{ 2, "connectivity", { "b" } });

    const std::string printed = cli::format_text(report);
    REQUIRE(cli::exit_code(report) == 2);
    REQUIRE(printed.find("[2]") != std::string::npos);
    REQUIRE(printed.find("[4]") != std::string::npos);
    REQUIRE(cli::format_json(report).find("\"exit_code\":2") != std::string::npos);
}

TEST_CASE("cli_validate: the exit code agrees with the reported status in both modes")
{
    std::string text;
    std::string json;
    const int text_code = run_validate_on(fixture("multi_root.urdf"), "", text);
    const int json_code = run_validate_on(fixture("multi_root.urdf"), "json", json);

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
