#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>
#include <filesystem>

#if defined(MEIOS_CLI_BINARY) && !defined(_WIN32)
    #include <cstdio>
    #include <stdio.h>

    #include <sys/wait.h>
#endif

namespace
{

#if defined(MEIOS_CLI_BINARY) && !defined(_WIN32)
// The smoke cases drive real published descriptions, which are far too large to vendor. A
// developer points MEIOS_SMOKE_CORPUS_DIR at a local checkout to make them live; with no
// definition they report a skip, so the suite stays green on a machine without the corpus.
std::filesystem::path corpus_root()
{
    #ifdef MEIOS_SMOKE_CORPUS_DIR
    return std::filesystem::path{MEIOS_SMOKE_CORPUS_DIR};
    #else
    return {};
    #endif
}

struct shell_result
{
    int status;
    std::string output;
};

shell_result run_cli(const std::string &command)
{
    const std::string piped = command + " 2>&1";
    std::string output;
    FILE *pipe = popen(piped.c_str(), "r");
    if(pipe == nullptr)
        return {-1, {}};
    char buffer[4096];
    while(std::fgets(buffer, sizeof(buffer), pipe) != nullptr)
        output += buffer;
    const int closed = pclose(pipe);
    return {WIFEXITED(closed) ? WEXITSTATUS(closed) : -1, std::move(output)};
}
#endif

}

TEST_CASE("the real UR description flattens its yaml under the python evaluator", "[urdf][yaml][flatten][smoke]")
{
#if defined(MEIOS_CLI_BINARY) && !defined(_WIN32)
    const std::filesystem::path root = corpus_root();
    const std::filesystem::path ur   = root / "Universal_Robots_ROS2_Description/urdf/ur.urdf.xacro";
    if(root.empty() || !std::filesystem::exists(ur))
    {
        SUCCEED("UR corpus absent — smoke skipped");
        return;
    }
    const std::string command = std::string(MEIOS_CLI_BINARY) + " flatten " + ur.string() + " --package-path " + root.string() + " ur_type:=ur5e name:=ur --eval python";
    const shell_result out    = run_cli(command);
    REQUIRE(out.output.find("name 'xacro' is not defined") == std::string::npos);
    REQUIRE(out.output.find("ConstructorError") == std::string::npos);
    REQUIRE(out.status == 0);
    REQUIRE(out.output.find("<robot") != std::string::npos);
#else
    SUCCEED("CLI binary or POSIX shell unavailable — smoke skipped");
#endif
}

TEST_CASE("a ROS1 kuka description flattens through the core evaluator", "[urdf][flatten][smoke]")
{
#if defined(MEIOS_CLI_BINARY) && !defined(_WIN32)
    const std::filesystem::path root = corpus_root();
    const std::filesystem::path kuka = root / "kuka_experimental/kuka_kr16_support/urdf/kr16_2.xacro";
    if(root.empty() || !std::filesystem::exists(kuka))
    {
        SUCCEED("kuka corpus absent — smoke skipped");
        return;
    }
    const std::string command = std::string(MEIOS_CLI_BINARY) + " flatten " + kuka.string() + " --package-path " + root.string();
    const shell_result out    = run_cli(command);
    REQUIRE(out.status == 0);
    REQUIRE(out.output.find("<robot") != std::string::npos);
#else
    SUCCEED("CLI binary or POSIX shell unavailable — smoke skipped");
#endif
}
