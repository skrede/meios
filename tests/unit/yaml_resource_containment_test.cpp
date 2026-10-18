#include "yaml_resource_fixture.h"

#include <meios/diagnostic/log_sink.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <filesystem>
#include <functional>
#include <system_error>

using yaml_resource_test::probe;
using yaml_resource_test::says;
using yaml_resource_test::seed_tree;
using yaml_resource_test::tally;

TEST_CASE("a bare spec resolves against the document handed to the call", "[yaml][resource]")
{
    const std::filesystem::path root = seed_tree();
    tally counts;
    meios::log_sink_f sink{std::ref(counts)};
    probe under(sink, root, {});

    REQUIRE(under.loader("cfg.yaml", root / "docs" / "robot.xacro").value() == std::string("from: docs"));
    REQUIRE(under.loader("cfg.yaml", root / "other" / "robot.xacro").value() == std::string("from: other"));
    REQUIRE(under.loader("sub/cfg.yaml", root / "docs" / "robot.xacro").value() == std::string("from: docs-sub"));
    REQUIRE(counts.errors == 0);

    std::filesystem::remove_all(root);
}

TEST_CASE("a bare relative document name still reaches a resource beside it", "[yaml][resource]")
{
    const std::filesystem::path root = seed_tree();
    tally counts;
    meios::log_sink_f sink{std::ref(counts)};
    probe under(sink, root, {});

    const std::filesystem::path saved = std::filesystem::current_path();
    std::filesystem::current_path(root / "docs");
    const meios::text_resource_result text = under.loader("cfg.yaml", "robot.xacro");
    std::filesystem::current_path(saved);
    std::filesystem::remove_all(root);

    REQUIRE(text.value() == std::string("from: docs"));
    REQUIRE(counts.errors == 0);
}

TEST_CASE("a bare spec with no document resolves only inside a configured root", "[yaml][resource]")
{
    const std::filesystem::path root = seed_tree();
    tally counts;
    meios::log_sink_f sink{std::ref(counts)};
    probe under(sink, root, {root});

    REQUIRE(under.loader("loose.yaml", {}).value() == std::string("from: root"));
    REQUIRE_FALSE(under.loader("docs/absent.yaml", {}).has_value());
    REQUIRE(counts.errors == 1);

    std::filesystem::remove_all(root);
}

TEST_CASE("an absolute spec is judged by where it lands, not by how it looks", "[yaml][resource]")
{
    const std::filesystem::path root = seed_tree();
    tally counts;
    meios::log_sink_f sink{std::ref(counts)};
    probe under(sink, root, {});

    const std::filesystem::path document = root / "docs" / "robot.xacro";
    REQUIRE(under.loader((root / "docs" / "cfg.yaml").string(), document).value() == std::string("from: docs"));
    REQUIRE(counts.errors == 0);

    REQUIRE_FALSE(under.loader("/etc/passwd", document).has_value());
    REQUIRE(counts.errors == 1);
    REQUIRE(says(counts, "/etc/passwd"));

    std::filesystem::remove_all(root);
}

TEST_CASE("a traversal spec is refused exactly once", "[yaml][resource]")
{
    const std::filesystem::path root = seed_tree();
    tally counts;
    meios::log_sink_f sink{std::ref(counts)};
    probe under(sink, root, {root});

    REQUIRE_FALSE(under.loader("../../../etc/passwd", root / "docs" / "robot.xacro").has_value());
    REQUIRE(counts.errors == 1);

    std::filesystem::remove_all(root);
}

TEST_CASE("a link whose real target escapes the document directory is refused", "[yaml][resource]")
{
    const std::filesystem::path root = seed_tree();
    std::error_code ec;
    std::filesystem::create_symlink(root / "outside" / "secret.yaml", root / "docs" / "link.yaml", ec);
    if(ec)
    {
        std::filesystem::remove_all(root);
        SUCCEED("this host cannot create a symlink");
        return;
    }

    tally counts;
    meios::log_sink_f sink{std::ref(counts)};
    probe under(sink, root, {});

    REQUIRE_FALSE(under.loader("link.yaml", root / "docs" / "robot.xacro").has_value());
    REQUIRE(counts.errors == 1);

    std::filesystem::remove_all(root);
}
