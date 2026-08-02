#include "meios/urdf/yaml_resource.h"

#include <meios/io/memory_source.h>
#include <meios/io/source_handle.h>
#include <meios/io/source_stack.h>
#include <meios/io/directory_source.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>

#include <catch2/catch_test_macros.hpp>

#include <random>
#include <string>
#include <vector>
#include <fstream>
#include <utility>
#include <optional>
#include <filesystem>
#include <functional>
#include <system_error>

namespace
{

struct tally
{
    int errors{ 0 };
    std::vector<std::string> messages{};

    void operator()(meios::level lvl, const std::string &message)
    {
        if(lvl == meios::level::error)
            ++errors;
        messages.push_back(message);
    }

    void operator()(meios::level lvl, const meios::source_location &, const std::string &message)
    {
        (*this)(lvl, message);
    }
};

bool says(const tally &counts, std::string_view fragment)
{
    for(const std::string &message : counts.messages)
        if(message.find(fragment) != std::string::npos)
            return true;
    return false;
}

// Owns everything the loader captures by reference, so a case's loader stays valid for
// exactly as long as the case does.
struct probe
{
    probe(meios::log_sink &log, const std::filesystem::path &root,
          std::vector<std::filesystem::path> configured)
        : sources(), roots(std::move(configured)),
          loader(meios::detail::make_yaml_text_loader(sources, roots, log))
    {
        if(!root.empty())
            sources.push_back(meios::source_handle(meios::directory_source(root, log)));
    }

    meios::source_stack sources;
    std::vector<std::filesystem::path> roots;
    meios::text_resource_loader loader;
};

std::filesystem::path fresh_dir()
{
    std::random_device device;
    for(;;)
    {
        std::filesystem::path candidate = std::filesystem::temp_directory_path()
                                        / ("meios-yaml-" + std::to_string(device()));
        if(!std::filesystem::exists(candidate))
            return candidate;
    }
}

// pkg/ is the package a source layer serves; docs/ and other/ are two documents holding
// same-named neighbours, and outside/ is the target a link must not be able to reach.
std::filesystem::path seed_tree()
{
    std::filesystem::path root = fresh_dir();
    std::filesystem::create_directories(root / "pkg");
    std::filesystem::create_directories(root / "docs" / "sub");
    std::filesystem::create_directories(root / "other");
    std::filesystem::create_directories(root / "outside");
    std::ofstream(root / "pkg" / "cfg.yaml") << "from: package";
    std::ofstream(root / "docs" / "cfg.yaml") << "from: docs";
    std::ofstream(root / "docs" / "sub" / "cfg.yaml") << "from: docs-sub";
    std::ofstream(root / "other" / "cfg.yaml") << "from: other";
    std::ofstream(root / "outside" / "secret.yaml") << "from: outside";
    std::ofstream(root / "loose.yaml") << "from: root";
    return root;
}

}

TEST_CASE("a package-qualified spec resolves through the source stack", "[yaml][resource]")
{
    const std::filesystem::path root = seed_tree();
    tally counts;
    meios::log_sink_f sink{ std::ref(counts) };
    probe under(sink, root, {});

    REQUIRE(under.loader("package://pkg/cfg.yaml", root / "docs" / "robot.xacro")
            == std::string("from: package"));
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
    REQUIRE(under.loader("$(find pkg)/cfg.yaml", document)
            == under.loader("package://pkg/cfg.yaml", document));
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

TEST_CASE("a bare spec resolves against the document handed to the call", "[yaml][resource]")
{
    const std::filesystem::path root = seed_tree();
    tally counts;
    meios::log_sink_f sink{ std::ref(counts) };
    probe under(sink, root, {});

    REQUIRE(under.loader("cfg.yaml", root / "docs" / "robot.xacro") == std::string("from: docs"));
    REQUIRE(under.loader("cfg.yaml", root / "other" / "robot.xacro") == std::string("from: other"));
    REQUIRE(under.loader("sub/cfg.yaml", root / "docs" / "robot.xacro")
            == std::string("from: docs-sub"));
    REQUIRE(counts.errors == 0);

    std::filesystem::remove_all(root);
}

TEST_CASE("a bare relative document name still reaches a resource beside it", "[yaml][resource]")
{
    const std::filesystem::path root = seed_tree();
    tally counts;
    meios::log_sink_f sink{ std::ref(counts) };
    probe under(sink, root, {});

    const std::filesystem::path saved = std::filesystem::current_path();
    std::filesystem::current_path(root / "docs");
    const std::optional<std::string> text = under.loader("cfg.yaml", "robot.xacro");
    std::filesystem::current_path(saved);
    std::filesystem::remove_all(root);

    REQUIRE(text == std::string("from: docs"));
    REQUIRE(counts.errors == 0);
}

TEST_CASE("a bare spec with no document resolves only inside a configured root", "[yaml][resource]")
{
    const std::filesystem::path root = seed_tree();
    tally counts;
    meios::log_sink_f sink{ std::ref(counts) };
    probe under(sink, root, { root });

    REQUIRE(under.loader("loose.yaml", {}) == std::string("from: root"));
    REQUIRE_FALSE(under.loader("docs/absent.yaml", {}).has_value());
    REQUIRE(counts.errors == 1);

    std::filesystem::remove_all(root);
}

TEST_CASE("an absolute spec is judged by where it lands, not by how it looks", "[yaml][resource]")
{
    const std::filesystem::path root = seed_tree();
    tally counts;
    meios::log_sink_f sink{ std::ref(counts) };
    probe under(sink, root, {});

    const std::filesystem::path document = root / "docs" / "robot.xacro";
    REQUIRE(under.loader((root / "docs" / "cfg.yaml").string(), document)
            == std::string("from: docs"));
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
    meios::log_sink_f sink{ std::ref(counts) };
    probe under(sink, root, { root });

    REQUIRE_FALSE(
        under.loader("../../../etc/passwd", root / "docs" / "robot.xacro").has_value());
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
    meios::log_sink_f sink{ std::ref(counts) };
    probe under(sink, root, {});

    REQUIRE_FALSE(under.loader("link.yaml", root / "docs" / "robot.xacro").has_value());
    REQUIRE(counts.errors == 1);

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

    REQUIRE(loader("package://pkg/cfg.yaml", {}) == std::string("from: bytes"));
    REQUIRE(counts.errors == 0);
}
