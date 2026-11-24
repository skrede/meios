#ifndef HPP_GUARD_MEIOS_TESTS_URDF_YAML_FLATTEN_FIXTURE_H
#define HPP_GUARD_MEIOS_TESTS_URDF_YAML_FLATTEN_FIXTURE_H

#include <meios/eval/python_evaluator.h>

#include <meios/urdf.h>
#include <meios/model.h>
#include <meios/xacro.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <string_view>

namespace
{

inline std::filesystem::path fixture(std::string_view name)
{
    return std::filesystem::path{MEIOS_URDF_FIXTURE_DIR} / name;
}

inline std::vector<std::filesystem::path> fixture_roots()
{
    return {std::filesystem::path{MEIOS_URDF_FIXTURE_DIR}};
}

inline const meios::joint<double> *find_joint(const meios::model<double> &robot, std::string_view name)
{
    for(const meios::joint<double> &j : robot.joints)
        if(j.name == name)
            return &j;
    return nullptr;
}

// Builds the options in a frame that has returned by the time load() runs: the backend
// is owned by the options, so no caller has to keep storage alive alongside them. A
// description names its own auxiliary resources, so nothing is seeded here.
inline meios::load_options python_options()
{
    meios::load_options opts;
    opts.backend = std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
    return opts;
}

struct journal
{
    int errors{0};
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

inline void expect_shoulder(const attempt &got, double height, double upper)
{
    REQUIRE(got.robot.has_value());
    REQUIRE(got.errors == 0);
    const meios::joint<double> *shoulder = find_joint(*got.robot, "shoulder_joint");
    REQUIRE(shoulder != nullptr);
    REQUIRE(shoulder->origin.translation.z == Catch::Approx(height));
    REQUIRE(shoulder->limits.has_value());
    REQUIRE(shoulder->limits->upper == Catch::Approx(upper));
}

}

#endif
