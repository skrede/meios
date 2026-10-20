#include "verbs/verbs.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
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

// Redirects std::cout for the scope of one verb call so a test can assert on the
// bytes a verb writes, restoring the stream on destruction.
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

std::string run_info(const std::string &document, const std::string &format)
{
    verb_context ctx;
    ctx.id = "info";
    ctx.positionals = { fixture(document) };
    if(!format.empty())
        ctx.value_flags["--format"] = format;
    cout_capture out;
    REQUIRE(cli::run_info(ctx) == 0);
    return out.str();
}

bool has_line(const std::string &printed, const std::string &line)
{
    return ("\n" + printed).find("\n" + line + "\n") != std::string::npos;
}

void require_counts(const std::string &document, std::size_t total, std::size_t independent)
{
    const std::string text = run_info(document, "");
    REQUIRE(has_line(text, "total_dof: " + std::to_string(total)));
    REQUIRE(has_line(text, "independent_dof: " + std::to_string(independent)));
    const std::string json = run_info(document, "json");
    REQUIRE(json.find("\"total_dof\":" + std::to_string(total) + ",") != std::string::npos);
    REQUIRE(json.find("\"independent_dof\":" + std::to_string(independent) + ",") != std::string::npos);
}

}

TEST_CASE("cli_info: info reports the total and the independent degrees of freedom")
{
    const std::string text = run_info("branched_all_joints.urdf", "");
    REQUIRE(has_line(text, "total_dof: 12"));
    REQUIRE(has_line(text, "independent_dof: 12"));
    REQUIRE(("\n" + text).find("\ndof:") == std::string::npos);
}

TEST_CASE("cli_info: info --format json carries both counts and drops the single count")
{
    const std::string json = run_info("branched_all_joints.urdf", "json");
    REQUIRE(json.find("\"total_dof\":12,") != std::string::npos);
    REQUIRE(json.find("\"independent_dof\":12,") != std::string::npos);
    REQUIRE(json.find("\"dof\":") == std::string::npos);
}

TEST_CASE("cli_info: a floating, a planar and two revolute joints, one mimicking the other, report eleven and ten")
{
    require_counts("floating_planar_mimic.urdf", 11, 10);
}

TEST_CASE("cli_info: a mimic removes one coordinate from a planar joint and none from a fixed one")
{
    require_counts("mimic_on_fixed_and_planar.urdf", 4, 3);
}
