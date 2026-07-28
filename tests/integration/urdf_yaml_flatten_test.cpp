#include <meios/eval/python_evaluator.h>

#include <meios/urdf.h>
#include <meios/model.h>
#include <meios/xacro.h>

#include <meios/io/source_stack.h>
#include <meios/io/memory_source.h>
#include <meios/io/source_handle.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <string>
#include <vector>
#include <utility>
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

std::vector<std::filesystem::path> fixture_roots()
{
    return { std::filesystem::path{ MEIOS_URDF_FIXTURE_DIR } };
}

const meios::joint<double> *find_joint(const meios::model<double> &robot, std::string_view name)
{
    for(const meios::joint<double> &j : robot.joints)
        if(j.name == name)
            return &j;
    return nullptr;
}

// Builds the options in a frame that has returned by the time load() runs: the backend
// is owned by the options, so no caller has to keep storage alive alongside them. A
// description names its own auxiliary resources, so nothing is seeded here.
meios::load_options python_options()
{
    meios::load_options opts;
    opts.backend = std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
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

struct journal
{
    int errors{ 0 };
    std::string report;

    void record(meios::level lvl, const std::string &message)
    {
        if(lvl != meios::level::error)
            return;
        ++errors;
        report += message + '\n';
    }

    void operator()(meios::level lvl, const std::string &message)
    {
        record(lvl, message);
    }

    void operator()(meios::level lvl, const meios::source_location &, const std::string &message)
    {
        record(lvl, message);
    }
};

struct attempt
{
    int errors;
    std::string report;
    std::optional<meios::model<double>> robot;
};

attempt flatten(const std::filesystem::path &path, meios::eval_policy policy,
                const std::vector<std::filesystem::path> &roots)
{
    journal book;
    meios::log_sink_f sink{ std::ref(book) };
    meios::load_options opts = python_options();
    opts.eval                = policy;
    opts.package_roots       = roots;
    meios::expected<meios::load_result, meios::load_error> loaded = meios::load(path, opts, sink);
    if(!loaded)
        return { book.errors, book.report + loaded.error().message, std::nullopt };
    return { book.errors, std::move(book.report), std::move(loaded->robot) };
}

attempt flatten_over(const std::filesystem::path &path, meios::source_stack &sources)
{
    journal book;
    meios::log_sink_f sink{ std::ref(book) };
    meios::capturing_log_sink capture{ sink };
    const meios::load_options opts = python_options();
    meios::expected<meios::load_result, meios::load_error> loaded =
        meios::load(path, opts, sources, capture);
    if(!loaded)
        return { book.errors, book.report + loaded.error().message, std::nullopt };
    return { book.errors, std::move(book.report), std::move(loaded->robot) };
}

void expect_shoulder(const attempt &got, double height, double upper)
{
    REQUIRE(got.robot.has_value());
    REQUIRE(got.errors == 0);
    const meios::joint<double> *shoulder = find_joint(*got.robot, "shoulder_joint");
    REQUIRE(shoulder != nullptr);
    REQUIRE(shoulder->origin.translation.z == Catch::Approx(height));
    REQUIRE(shoulder->limits.has_value());
    REQUIRE(shoulder->limits->upper == Catch::Approx(upper));
}

// The diagnostic deliberately echoes the offending expression, so the leak probe looks for
// the target file's own content instead: nothing of /etc/passwd may reach any output.
void expect_refused(const attempt &got)
{
    REQUIRE_FALSE(got.robot.has_value());
    REQUIRE(got.errors >= 1);
    REQUIRE(got.report.find("uncontained-yaml-path") != std::string::npos);
    REQUIRE(got.report.find("root:") == std::string::npos);
    REQUIRE(got.report.find("/bin/") == std::string::npos);
}

#if defined(MEIOS_CLI_BINARY) && !defined(_WIN32)
// The smoke cases drive real published descriptions, which are far too large to vendor. A
// developer points MEIOS_SMOKE_CORPUS_DIR at a local checkout to make them live; with no
// definition they report a skip, so the suite stays green on a machine without the corpus.
std::filesystem::path corpus_root()
{
#ifdef MEIOS_SMOKE_CORPUS_DIR
    return std::filesystem::path{ MEIOS_SMOKE_CORPUS_DIR };
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

    const meios::load_options opts = python_options();

    const meios::expected<meios::load_result, meios::load_error> loaded =
        meios::load(fixture("yaml_arm/arm.urdf.xacro"), opts, sink);

    REQUIRE(loaded.has_value());
    const meios::model<double> &robot = loaded->robot;
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

TEST_CASE("a package-qualified yaml spec resolves through the source stack",
          "[urdf][yaml][flatten]")
{
    expect_shoulder(flatten(fixture("yaml_pkg/package.urdf.xacro"), meios::eval_policy::fail,
                            fixture_roots()),
                    0.25, 1.5);
}

TEST_CASE("a find token written inside an expression resolves the same way",
          "[urdf][yaml][flatten]")
{
    expect_shoulder(flatten(fixture("yaml_pkg/find.urdf.xacro"), meios::eval_policy::fail,
                            fixture_roots()),
                    0.25, 1.5);
}

// The decoy beside the top-level document carries different values, so reading the wrong
// directory would succeed with the wrong numbers rather than merely fail to resolve.
TEST_CASE("a bare yaml spec resolves against the including document, not the top-level one",
          "[urdf][yaml][flatten]")
{
    const attempt got =
        flatten(fixture("yaml_pkg/includer.urdf.xacro"), meios::eval_policy::fail, fixture_roots());
    expect_shoulder(got, 0.75, 2.5);
    REQUIRE(find_joint(*got.robot, "shoulder_joint")->origin.translation.z
            != Catch::Approx(0.11));
}

TEST_CASE("an absolute yaml spec fails the load and leaks no file content",
          "[urdf][yaml][flatten]")
{
    expect_refused(flatten(fixture("yaml_pkg/absolute.urdf.xacro"), meios::eval_policy::fail,
                           fixture_roots()));
}

TEST_CASE("a traversal-escaping yaml spec fails the load and leaks no file content",
          "[urdf][yaml][flatten]")
{
    expect_refused(flatten(fixture("yaml_pkg/traversal.urdf.xacro"), meios::eval_policy::fail,
                           fixture_roots()));
}

TEST_CASE("a yaml refusal still fails the load under the most lenient policy",
          "[urdf][yaml][flatten]")
{
    expect_refused(flatten(fixture("yaml_pkg/absolute.urdf.xacro"), meios::eval_policy::skip,
                           fixture_roots()));
    expect_refused(flatten(fixture("yaml_pkg/traversal.urdf.xacro"), meios::eval_policy::skip,
                           fixture_roots()));
}

TEST_CASE("an accepted yaml spec loads identically under the most lenient policy",
          "[urdf][yaml][flatten]")
{
    expect_shoulder(flatten(fixture("yaml_pkg/package.urdf.xacro"), meios::eval_policy::skip,
                            fixture_roots()),
                    0.25, 1.5);
}

TEST_CASE("an in-memory source layer serves a working configuration", "[urdf][yaml][flatten]")
{
    meios::log_sink silent;
    meios::memory_source layer{ silent };
    layer.add("memory_pkg", "config.yaml", "joints:\n  shoulder:\n    height: 0.5\n"
                                           "    lower: !degrees -30\n"
                                           "    upper: 3.0\n"
                                           "    effort: 50.0\n"
                                           "    velocity: 1.0\n");

    meios::source_stack sources;
    sources.push_back(meios::source_handle(std::move(layer)));
    expect_shoulder(flatten_over(fixture("yaml_pkg/bytes.urdf.xacro"), sources), 0.5, 3.0);
}

TEST_CASE("the real UR description clears the yaml parity layer", "[urdf][yaml][flatten][smoke]")
{
#if defined(MEIOS_CLI_BINARY) && !defined(_WIN32)
    const std::filesystem::path root = corpus_root();
    const std::filesystem::path ur =
        root / "Universal_Robots_ROS2_Description/urdf/ur.urdf.xacro";
    if(root.empty() || !std::filesystem::exists(ur))
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
    const std::filesystem::path root = corpus_root();
    const std::filesystem::path kuka =
        root / "kuka_experimental/kuka_kr16_support/urdf/kr16_2.xacro";
    if(root.empty() || !std::filesystem::exists(kuka))
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
