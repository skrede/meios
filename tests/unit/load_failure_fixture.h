#ifndef HPP_GUARD_MEIOS_UNIT_LOAD_FAILURE_FIXTURE_H
#define HPP_GUARD_MEIOS_UNIT_LOAD_FAILURE_FIXTURE_H

#include <meios/urdf.h>
#include <meios/model.h>

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/capturing_log_sink.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <chrono>
#include <fstream>
#include <filesystem>

namespace
{

std::filesystem::path fixture(const std::string &name)
{
    return std::filesystem::path{ MEIOS_URDF_FIXTURE_DIR } / name;
}

struct diagnostic_recorder
{
    std::vector<meios::captured_diagnostic> &out;

    void operator()(meios::level, const std::string &message)
    {
        out.push_back({ meios::diagnostic_code::unspecified, meios::source_location{}, message });
    }

    void operator()(meios::level, const meios::source_location &loc, const std::string &message)
    {
        out.push_back({ meios::diagnostic_code::unspecified, loc, message });
    }

    void operator()(meios::level, meios::diagnostic_code code, const meios::source_location &loc,
                    const std::string &message)
    {
        out.push_back({ code, loc, message });
    }
};

}

#endif
