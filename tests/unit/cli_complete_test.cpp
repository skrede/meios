#include "complete.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <filesystem>

using namespace meios;

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

std::string complete(const std::vector<std::string> &words)
{
    cout_capture out;
    cli::run_complete(words);
    return out.str();
}

}

TEST_CASE("cli_complete: a model position falls back to filesystem completion")
{
    const std::string out = complete({ "flatten", "" });
    REQUIRE(out.find(":0") != std::string::npos);
    REQUIRE(out.find(":4") == std::string::npos);
}

TEST_CASE("cli_complete: tree --root enumerates live link names")
{
    const std::string out = complete({ "tree", fixture("branched_all_joints.urdf"), "--root", "" });
    REQUIRE(out.find("base_link") != std::string::npos);
    REQUIRE(out.find("slider") != std::string::npos);
    REQUIRE(out.find(":4") != std::string::npos);
}

TEST_CASE("cli_complete: resolve enumerates live link and joint names")
{
    const std::string out = complete({ "resolve", fixture("branched_all_joints.urdf"), "" });
    REQUIRE(out.find("torso") != std::string::npos);
    REQUIRE(out.find("torso_to_arm") != std::string::npos);
    REQUIRE(out.find(":4") != std::string::npos);
}

TEST_CASE("cli_complete: an arg position enumerates the document's args")
{
    const std::string out = complete({ "args", fixture("args_doc.xacro"), "" });
    REQUIRE(out.find("mass") != std::string::npos);
    REQUIRE(out.find("prefix") != std::string::npos);
    REQUIRE(out.find(":4") != std::string::npos);
}

TEST_CASE("cli_complete: a fallback position performs no parse")
{
    const std::string out = complete({ "resolve", fixture("branched_all_joints.urdf"),
                                       "base_link", "" });
    REQUIRE(out.find(":0") != std::string::npos);
    REQUIRE(out.find("torso") == std::string::npos);
}
