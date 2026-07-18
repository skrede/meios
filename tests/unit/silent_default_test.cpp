#include <meios/urdf.h>
#include <meios/model.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <sstream>
#include <iostream>
#include <streambuf>
#include <filesystem>

namespace
{

std::filesystem::path fixture(const std::string &name)
{
    return std::filesystem::path{ MEIOS_URDF_FIXTURE_DIR } / name;
}

}

TEST_CASE("a default load writes nothing to cerr or cout", "[urdf][silent]")
{
    std::ostringstream cerr_capture;
    std::ostringstream cout_capture;
    std::streambuf *saved_cerr = std::cerr.rdbuf(cerr_capture.rdbuf());
    std::streambuf *saved_cout = std::cout.rdbuf(cout_capture.rdbuf());

    meios::load_options opts;
    const meios::expected<meios::model<double>, meios::load_error> result =
        meios::load(fixture("test-desc.urdf"), opts);

    std::cerr.rdbuf(saved_cerr);
    std::cout.rdbuf(saved_cout);

    REQUIRE(result.has_value());
    REQUIRE(cerr_capture.str().empty());
    REQUIRE(cout_capture.str().empty());
}

TEST_CASE("to_string covers the trace and debug levels", "[diagnostic][level]")
{
    REQUIRE(meios::to_string(meios::level::trace) == "trace");
    REQUIRE(meios::to_string(meios::level::debug) == "debug");
}
