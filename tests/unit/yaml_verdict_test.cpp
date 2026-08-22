// Which terminal verdict the containment fetcher reports is decided across every probe root, so a
// single-root case cannot observe the interaction that decides it.
#include "yaml_acquisition_fixture.h"

#include <meios/io/text_reader.h>

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>
#include <meios/diagnostic/operation_failure.h>
#include <meios/diagnostic/capturing_log_sink.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <system_error>

namespace
{

using acquisition_test::tree_guard;

std::string absent_in_docs(const tree_guard &tree)
{
    return (tree.path() / "docs" / "absent.yaml").string();
}

// What the reader itself determined for the same request, so the expectation names the
// classification rather than one platform's spelling of it.
std::string reader_reason(const std::filesystem::path &root, const std::filesystem::path &relative)
{
    const meios::text_read_result direct = meios::read_text_file_under(root, relative);
    REQUIRE_FALSE(direct.has_value());
    return meios::detail::read_failure_reason(direct.error());
}

void require_classified(const meios::capturing_log_sink &capture, const std::string &reason)
{
    REQUIRE(capture.errors() == 1);
    REQUIRE(capture.first()->code == meios::diagnostic_code::cannot_open);
    CHECK(capture.first()->message.find(reason) != std::string::npos);
    CHECK(capture.first()->message.find(std::error_code{}.message()) == std::string::npos);
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

TEST_CASE("a directory named through containment is refused by classification", "[yaml][verdict]")
{
    const tree_guard tree;
    meios::log_sink silent;
    meios::capturing_log_sink capture{ silent };
    meios::source_stack sources;
    const std::vector<std::filesystem::path> roots{ tree.path() };
    auto loader = meios::detail::make_yaml_text_loader(sources, roots, capture);

    REQUIRE_FALSE(loader("docs", {}).has_value());
    require_classified(capture, reader_reason(tree.path(), "docs"));
}

TEST_CASE("a directory named through a package source is refused by classification", "[yaml][verdict]")
{
    const tree_guard tree;
    std::filesystem::create_directories(tree.path() / "pkg" / "sub");
    meios::log_sink silent;
    meios::capturing_log_sink capture{ silent };
    meios::source_stack sources{ meios::directory_source{ tree.path(), capture } };
    const std::vector<std::filesystem::path> roots;
    auto loader = meios::detail::make_yaml_text_loader(sources, roots, capture);

    REQUIRE_FALSE(loader("package://pkg/sub", {}).has_value());
    require_classified(capture, reader_reason(tree.path(), std::filesystem::path("pkg") / "sub"));
}

TEST_CASE("a traversal that stays inside a root reads, one that leaves it is refused", "[yaml][verdict]")
{
    const tree_guard tree;
    meios::log_sink silent;
    meios::capturing_log_sink capture{ silent };
    meios::source_stack sources;
    const std::vector<std::filesystem::path> roots{ tree.path() };
    auto loader = meios::detail::make_yaml_text_loader(sources, roots, capture);

    REQUIRE(loader("pkg/../docs/cfg.yaml", {}) == std::optional<std::string>{ "value: 7" });
    REQUIRE(capture.size() == 0);

    meios::capturing_log_sink escaped{ silent };
    auto escaping_loader = meios::detail::make_yaml_text_loader(sources, roots, escaped);
    REQUIRE_FALSE(escaping_loader("../cfg.yaml", {}).has_value());
    REQUIRE(escaped.errors() == 1);
    CHECK(escaped.first()->code == meios::diagnostic_code::uncontained_asset);
}
