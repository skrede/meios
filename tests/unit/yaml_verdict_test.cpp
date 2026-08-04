// Which terminal verdict the containment fetcher reports is decided across every probe root, so a
// single-root case cannot observe the interaction that decides it.
#include "yaml_acquisition_fixture.h"

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>
#include <meios/diagnostic/operation_failure.h>
#include <meios/diagnostic/capturing_log_sink.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <optional>
#include <filesystem>

namespace
{

using acquisition_test::tree_guard;

std::string absent_in_docs(const tree_guard &tree)
{
    return (tree.path() / "docs" / "absent.yaml").string();
}

}

TEST_CASE("a spec contained under one root and outside another reports absence", "[yaml][verdict]")
{
    const tree_guard tree;
    meios::log_sink silent;
    meios::capturing_log_sink capture{ silent };
    meios::source_stack sources;
    const std::vector<std::filesystem::path> roots{ tree.path() / "docs", tree.path() / "pkg" };
    auto loader = meios::detail::make_yaml_text_loader(sources, roots, capture);

    REQUIRE_FALSE(loader(absent_in_docs(tree), {}).has_value());
    REQUIRE(capture.errors() == 1);
    CHECK(capture.first()->code == meios::diagnostic_code::unresolved_asset);
    CHECK_FALSE(capture.first()->cause.has_value());
}

TEST_CASE("a spec outside every root still reports a containment refusal", "[yaml][verdict]")
{
    const tree_guard tree;
    meios::log_sink silent;
    meios::capturing_log_sink capture{ silent };
    meios::source_stack sources;
    const std::vector<std::filesystem::path> roots{ tree.path() / "docs", tree.path() / "pkg" };
    auto loader = meios::detail::make_yaml_text_loader(sources, roots, capture);

    REQUIRE_FALSE(loader((tree.path() / "outside.yaml").string(), {}).has_value());
    REQUIRE(capture.errors() == 1);
    CHECK(capture.first()->code == meios::diagnostic_code::uncontained_asset);
}

TEST_CASE("a spec contained under every root but held by none reports absence", "[yaml][verdict]")
{
    const tree_guard tree;
    meios::log_sink silent;
    meios::capturing_log_sink capture{ silent };
    meios::source_stack sources;
    const std::vector<std::filesystem::path> roots{ tree.path(), tree.path() / "docs" };
    auto loader = meios::detail::make_yaml_text_loader(sources, roots, capture);

    REQUIRE_FALSE(loader(absent_in_docs(tree), {}).has_value());
    REQUIRE(capture.errors() == 1);
    CHECK(capture.first()->code == meios::diagnostic_code::unresolved_asset);
    CHECK_FALSE(capture.first()->cause.has_value());
}

TEST_CASE("a determined native failure outranks either containment verdict", "[yaml][verdict]")
{
    const tree_guard tree;
    acquisition_test::reader_script script;
    script.open_error = { 37, acquisition_test::category };
    acquisition_test::scripted_operations operations{ script };
    meios::log_sink silent;
    meios::capturing_log_sink capture{ silent };
    meios::source_stack sources;
    const std::vector<std::filesystem::path> roots{ tree.path() / "docs", tree.path() / "pkg" };
    acquisition_test::delivery_probe delivery;
    auto loader =
        meios::detail::make_yaml_text_loader(sources, roots, capture, operations, delivery);

    REQUIRE_FALSE(loader((tree.path() / "docs" / "cfg.yaml").string(), {}).has_value());
    REQUIRE(capture.first()->cause.has_value());
    CHECK(capture.first()->code == meios::diagnostic_code::cannot_open);
    CHECK(capture.first()->cause->operation == meios::operation_kind::open);
    CHECK(capture.first()->cause->native == script.open_error);
    CHECK(delivery.count == 0);
}
