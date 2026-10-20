#include "yaml_resource_fixture.h"

#include <meios/io/source_stack.h>
#include <meios/io/memory_source.h>
#include <meios/io/source_handle.h>

#include <meios/diagnostic/log_sink.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <fstream>
#include <utility>
#include <filesystem>
#include <functional>
#include <system_error>

using yaml_resource_test::probe;
using yaml_resource_test::says;
using yaml_resource_test::seed_tree;
using yaml_resource_test::tally;

TEST_CASE("a package-qualified spec resolves through the source stack", "[yaml][resource]")
{
    const std::filesystem::path root = seed_tree();
    tally counts;
    meios::log_sink_f sink{std::ref(counts)};
    probe under(sink, root, {});

    REQUIRE(under.loader("package://pkg/cfg.yaml", root / "docs" / "robot.xacro").value() == std::string("from: package"));
    REQUIRE(counts.errors == 0);

    std::filesystem::remove_all(root);
}

TEST_CASE("a package URI carrying no relative part refuses loudly", "[yaml][resource]")
{
    const std::filesystem::path root = seed_tree();
    tally counts;
    meios::log_sink_f sink{std::ref(counts)};
    probe under(sink, root, {});

    const meios::text_resource_result malformed = under.loader("package://pkg", root / "docs" / "robot.xacro");
    REQUIRE_FALSE(malformed.has_value());
    CHECK(malformed.error() == meios::text_resource_failure::unreachable);
    REQUIRE(counts.errors == 1);
    REQUIRE(says(counts, "package://pkg"));

    std::filesystem::remove_all(root);
}

TEST_CASE("a find spec takes the same resolution path as the package form", "[yaml][resource]")
{
    const std::filesystem::path root = seed_tree();
    tally counts;
    meios::log_sink_f sink{std::ref(counts)};
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
    meios::log_sink_f sink{std::ref(counts)};
    probe under(sink, root, {});

    const meios::text_resource_result missing = under.loader("package://pkg/missing.yaml", root / "docs" / "robot.xacro");
    REQUIRE_FALSE(missing.has_value());
    CHECK(missing.error() == meios::text_resource_failure::unreachable);
    REQUIRE(counts.errors == 1);
    REQUIRE(says(counts, "package://pkg/missing.yaml"));

    std::filesystem::remove_all(root);
}

TEST_CASE("a byte-backed layer serves the resource with no path involved", "[yaml][resource]")
{
    tally counts;
    meios::log_sink_f sink{std::ref(counts)};
    meios::source_stack sources;
    meios::memory_source layer{sink};
    layer.add("pkg", "cfg.yaml", "from: bytes");
    sources.push_back(meios::source_handle(std::move(layer)));

    const std::vector<std::filesystem::path> roots;
    const meios::text_resource_loader loader = meios::detail::make_yaml_text_loader(sources, roots, sink);

    REQUIRE(loader("package://pkg/cfg.yaml", {}).value() == std::string("from: bytes"));
    REQUIRE(counts.errors == 0);
}

TEST_CASE("a relative spec in a byte-served document resolves only once its source has written the file", "[yaml][resource]")
{
    tally counts;
    meios::log_sink_f sink{std::ref(counts)};
    meios::source_stack sources;
    meios::memory_source layer{sink};
    layer.add("pkg", "urdf/a.xacro", "<robot/>").add("pkg", "urdf/c.yaml", "from: sibling");
    sources.push_back(meios::source_handle(std::move(layer)));
    const std::vector<std::filesystem::path> roots;
    const meios::text_resource_loader loader = meios::detail::make_yaml_text_loader(sources, roots, sink);
    const std::filesystem::path document     = sources.locate("pkg", "urdf/a.xacro", sink)->path();

    const meios::text_resource_result before = loader("c.yaml", document);
    REQUIRE_FALSE(before.has_value());
    CHECK(before.error() == meios::text_resource_failure::unreachable);
    REQUIRE(counts.errors == 1);

    REQUIRE(sources.locate("pkg", "urdf/c.yaml", sink).has_value());
    REQUIRE(loader("c.yaml", document).value() == std::string("from: sibling"));
    REQUIRE(counts.errors == 1);
}

TEST_CASE("a package-form spec naming a link inside its root is read through the root", "[yaml][resource]")
{
    const std::filesystem::path root = seed_tree();
    std::error_code ec;
    std::filesystem::create_symlink("cfg.yaml", root / "pkg" / "linked.yaml", ec);
    if(ec)
    {
        std::filesystem::remove_all(root);
        SUCCEED("this host cannot create a symlink");
        return;
    }
    tally counts;
    meios::log_sink_f sink{std::ref(counts)};
    probe under(sink, root, {});

    const meios::text_resource_result linked = under.loader("package://pkg/linked.yaml", root / "docs" / "robot.xacro");
    std::filesystem::remove_all(root);

    REQUIRE(linked.value() == std::string("from: package"));
    REQUIRE(counts.errors == 0);
}

TEST_CASE("a spelling through a file that is not a directory refuses, and no configured root's copy stands in", "[yaml][resource]")
{
#ifdef _WIN32
    SKIP("the Windows path API reduces .. before the filesystem sees the element that is not a directory");
#endif
    const std::filesystem::path root = seed_tree();
    std::filesystem::create_directories(root / "other" / "f.txt");
    std::ofstream(root / "docs" / "f.txt") << "plain";
    std::ofstream(root / "docs" / "x.yaml") << "from: beside_through_file";
    std::ofstream(root / "other" / "x.yaml") << "from: root_copy";
    std::ofstream(root / "other" / "f.txt" / "x.yaml") << "from: root_nested_yaml";
    std::ofstream(root / "other" / "y.yaml") << "from: genuine_miss";
    tally counts;
    meios::log_sink_f sink{std::ref(counts)};
    probe under(sink, root, {root / "other"});
    const std::filesystem::path document = root / "docs" / "robot.xacro";

    for(const char *spelling : {"f.txt/../x.yaml", "f.txt/x.yaml"})
    {
        DYNAMIC_SECTION(spelling)
        {
            const meios::text_resource_result through = under.loader(spelling, document);
            std::filesystem::remove_all(root);
            REQUIRE_FALSE(through.has_value());
            REQUIRE(through.error() == meios::text_resource_failure::unreachable);
            REQUIRE(counts.errors >= 1);
            REQUIRE(says(counts, std::make_error_code(std::errc::not_a_directory).message()));
        }
    }
    SECTION("a genuine miss beside the document is still read from the configured root")
    {
        const meios::text_resource_result missed = under.loader("y.yaml", document);
        std::filesystem::remove_all(root);
        REQUIRE(missed.value() == std::string("from: genuine_miss"));
    }
}
