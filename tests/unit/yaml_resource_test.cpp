#include "yaml_resource_fixture.h"

#include <meios/io/source_stack.h>
#include <meios/io/memory_source.h>
#include <meios/io/source_handle.h>

#include <meios/diagnostic/log_sink.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <utility>
#include <filesystem>
#include <functional>

using yaml_resource_test::probe;
using yaml_resource_test::says;
using yaml_resource_test::seed_tree;
using yaml_resource_test::tally;

TEST_CASE("a package-qualified spec resolves through the source stack", "[yaml][resource]")
{
    const std::filesystem::path root = seed_tree();
    tally counts;
    meios::log_sink_f sink{ std::ref(counts) };
    probe under(sink, root, {});

    REQUIRE(under.loader("package://pkg/cfg.yaml", root / "docs" / "robot.xacro").value() == std::string("from: package"));
    REQUIRE(counts.errors == 0);

    std::filesystem::remove_all(root);
}

TEST_CASE("a package URI carrying no relative part refuses loudly", "[yaml][resource]")
{
    const std::filesystem::path root = seed_tree();
    tally counts;
    meios::log_sink_f sink{ std::ref(counts) };
    probe under(sink, root, {});

    REQUIRE_FALSE(under.loader("package://pkg", root / "docs" / "robot.xacro").has_value());
    REQUIRE(counts.errors == 1);
    REQUIRE(says(counts, "package://pkg"));

    std::filesystem::remove_all(root);
}

TEST_CASE("a find spec takes the same resolution path as the package form", "[yaml][resource]")
{
    const std::filesystem::path root = seed_tree();
    tally counts;
    meios::log_sink_f sink{ std::ref(counts) };
    probe under(sink, root, {});

    const std::filesystem::path document = root / "docs" / "robot.xacro";
    REQUIRE(under.loader("$(find pkg)/cfg.yaml", document).value() == under.loader("package://pkg/cfg.yaml", document).value());
    REQUIRE(counts.errors == 0);

    std::filesystem::remove_all(root);
}

TEST_CASE("an unresolvable package spec refuses and names the spec", "[yaml][resource]")
{
    const std::filesystem::path root = seed_tree();
    tally counts;
    meios::log_sink_f sink{ std::ref(counts) };
    probe under(sink, root, {});

    REQUIRE_FALSE(
        under.loader("package://pkg/missing.yaml", root / "docs" / "robot.xacro").has_value());
    REQUIRE(counts.errors == 1);
    REQUIRE(says(counts, "package://pkg/missing.yaml"));

    std::filesystem::remove_all(root);
}

TEST_CASE("a byte-backed layer serves the resource with no path involved", "[yaml][resource]")
{
    tally counts;
    meios::log_sink_f sink{ std::ref(counts) };
    meios::source_stack sources;
    meios::memory_source layer{ sink };
    layer.add("pkg", "cfg.yaml", "from: bytes");
    sources.push_back(meios::source_handle(std::move(layer)));

    const std::vector<std::filesystem::path> roots;
    const meios::text_resource_loader loader =
        meios::detail::make_yaml_text_loader(sources, roots, sink);

    REQUIRE(loader("package://pkg/cfg.yaml", {}).value() == std::string("from: bytes"));
    REQUIRE(counts.errors == 0);
}
