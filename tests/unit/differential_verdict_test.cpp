#include "../differential_verdict.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <tuple>
#include <vector>

namespace
{

oracle::row row(std::initializer_list<std::string> fields)
{
    return oracle::row{ std::vector<std::string>(fields) };
}

}

TEST_CASE("an inventory id with no seen entry is the first missing verdict",
          "[differential_verdict]")
{
    const std::vector<oracle::row> inventory{ row({ "e01" }), row({ "e02" }), row({ "e03" }) };
    REQUIRE(differential::first_missing_verdict(inventory, { "e01", "e03" })
            == std::optional<std::string>("e02"));
    REQUIRE_FALSE(differential::first_missing_verdict(inventory, { "e01", "e02", "e03" }));
}

TEST_CASE("a manifest row whose id and both recorded columns match the observed text matches",
          "[differential_verdict]")
{
    const std::vector<oracle::row> manifest{ row({ "e01", "1", "True" }) };
    std::string detail;
    REQUIRE(differential::matches_manifest(manifest, "e01", "1", "True", detail));
    REQUIRE(detail.empty());
}

TEST_CASE("a manifest row whose id matches but whose recorded text has drifted does not match",
          "[differential_verdict]")
{
    const std::vector<oracle::row> manifest{ row({ "e01", "1", "True" }) };
    std::string detail;
    REQUIRE_FALSE(differential::matches_manifest(manifest, "e01", "1", "False", detail));
    REQUIRE(detail.find("e01") != std::string::npos);
    REQUIRE_FALSE(differential::matches_manifest(manifest, "e01", "2", "True", detail));
}

TEST_CASE("an id entirely absent from the manifest does not match", "[differential_verdict]")
{
    const std::vector<oracle::row> manifest{ row({ "e01", "1", "True" }) };
    std::string detail;
    REQUIRE_FALSE(differential::matches_manifest(manifest, "e02", "3", "True", detail));
    REQUIRE(detail.find("e02") != std::string::npos);
    REQUIRE(detail.find("not listed") != std::string::npos);
}

TEST_CASE("a manifest entry never present in the observed set is named stale",
          "[differential_verdict]")
{
    const std::vector<oracle::row> manifest{ row({ "e01", "1", "True" }),
                                             row({ "e02", "3", "True" }) };
    const std::vector<std::tuple<std::string, std::string, std::string>> observed{
        { "e01", "1", "True" }
    };
    REQUIRE(differential::stale_manifest_entries(manifest, observed)
            == std::vector<std::string>{ "e02" });
}
