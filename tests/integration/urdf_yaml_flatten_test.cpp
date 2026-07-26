#include <meios/eval/python_evaluator.h>

#include <meios/urdf.h>
#include <meios/model.h>
#include <meios/xacro.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <string_view>

#if defined(MEIOS_CLI_BINARY) && !defined(_WIN32)
#include <cstdio>
#include <sys/wait.h>
#endif

namespace
{

std::filesystem::path fixture(std::string_view name)
{
    return std::filesystem::path{ MEIOS_URDF_FIXTURE_DIR } / name;
}

const meios::joint<double> *find_joint(const meios::model<double> &robot, std::string_view name)
{
    for(const meios::joint<double> &j : robot.joints)
        if(j.name == name)
            return &j;
    return nullptr;
}

// Builds the options in a frame that has returned by the time load() runs: the backend
// is owned by the options, so no caller has to keep storage alive alongside them.
meios::load_options python_options(const std::filesystem::path &config)
{
    meios::load_options opts;
    opts.backend = std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
    opts.args["config_path"] = config.generic_string();
    return opts;
}

struct tally
{
    int &errors;

    void operator()(meios::level lvl, const std::string &)
    {
        if(lvl == meios::level::error)
            ++errors;
    }

    void operator()(meios::level lvl, const meios::source_location &, const std::string &)
    {
        if(lvl == meios::level::error)
            ++errors;
    }
};

#if defined(MEIOS_CLI_BINARY) && !defined(_WIN32)
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
        return { -1, {} };
    char buffer[4096];
    while(std::fgets(buffer, sizeof(buffer), pipe) != nullptr)
        output += buffer;
    const int closed = pclose(pipe);
    return { WIFEXITED(closed) ? WEXITSTATUS(closed) : -1, std::move(output) };
}
#endif

}

TEST_CASE("a yaml-driven arm flattens end-to-end through the python backend", "[urdf][yaml][flatten]")
{
    int errors = 0;
    meios::log_sink_f sink{ tally{ errors } };

    const meios::load_options opts = python_options(fixture("yaml_arm/config.yaml"));

    const meios::expected<meios::model<double>, meios::load_error> loaded =
        meios::load(fixture("yaml_arm/arm.urdf.xacro"), opts, sink);

    REQUIRE(loaded.has_value());
    const meios::model<double> &robot = *loaded;
    REQUIRE(errors == 0);
    REQUIRE(robot.links.size() == 2);

    const meios::joint<double> *shoulder = find_joint(robot, "shoulder_joint");
    REQUIRE(shoulder != nullptr);
    REQUIRE(shoulder->origin.translation.z == Catch::Approx(0.25));

    REQUIRE(shoulder->limits.has_value());
    REQUIRE(shoulder->limits->lower == Catch::Approx(-std::acos(-1.0) / 2.0));
    REQUIRE(shoulder->limits->upper == Catch::Approx(1.5));
    REQUIRE(shoulder->limits->effort == Catch::Approx(100.0));
    REQUIRE(shoulder->limits->velocity == Catch::Approx(2.0));
}

TEST_CASE("the real UR description clears the yaml parity layer", "[urdf][yaml][flatten][smoke]")
{
#if defined(MEIOS_CLI_BINARY) && !defined(_WIN32)
    const std::filesystem::path root = "/home/skrede/Workspace/ros/robots";
    const std::filesystem::path ur =
        root / "Universal_Robots_ROS2_Description/urdf/ur.urdf.xacro";
    if(!std::filesystem::exists(ur))
    {
        SUCCEED("UR corpus absent — smoke skipped");
        return;
    }
    const std::string command = std::string(MEIOS_CLI_BINARY) + " flatten " + ur.string()
        + " --package-path " + root.string() + " ur_type:=ur5e --eval python";
    const shell_result out = run_cli(command);
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
    const std::filesystem::path root = "/home/skrede/Workspace/ros/robots";
    const std::filesystem::path kuka =
        root / "kuka_experimental/kuka_kr16_support/urdf/kr16_2.xacro";
    if(!std::filesystem::exists(kuka))
    {
        SUCCEED("kuka corpus absent — smoke skipped");
        return;
    }
    const std::string command = std::string(MEIOS_CLI_BINARY) + " flatten " + kuka.string()
        + " --package-path " + root.string();
    const shell_result out = run_cli(command);
    REQUIRE(out.status == 0);
    REQUIRE(out.output.find("<robot") != std::string::npos);
#else
    SUCCEED("CLI binary or POSIX shell unavailable — smoke skipped");
#endif
}
