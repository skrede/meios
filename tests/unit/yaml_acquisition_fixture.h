#ifndef HPP_GUARD_MEIOS_TEST_YAML_ACQUISITION_FIXTURE_H
#define HPP_GUARD_MEIOS_TEST_YAML_ACQUISITION_FIXTURE_H

#include "acquisition_script.h"
#include "meios/urdf/yaml_resource.h"

#include <meios/io/scratch_dir.h>
#include <meios/io/source_stack.h>
#include <meios/io/memory_source.h>
#include <meios/io/source_lookup.h>
#include <meios/io/directory_source.h>

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/capturing_log_sink.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <fstream>
#include <utility>
#include <optional>
#include <filesystem>
#include <string_view>
#include <system_error>

namespace acquisition_test
{

class delivery_probe final : public meios::detail::yaml_text_delivery_probe
{
public:
    delivery_probe()
            : count(0)
    {
    }

    void delivered() override
    {
        ++count;
    }

    int count;
};

struct downstream_probe
{
    downstream_probe()
            : evaluations(0)
            , mutations(0)
            , sentinel("unchanged")
    {
    }

    void consume(const std::optional<std::string> &text)
    {
        if(!text)
            return;
        ++evaluations;
        ++mutations;
        sentinel = *text;
    }

    int evaluations;
    int mutations;
    std::string sentinel;
};

enum class source_answer
{
    absent,
    found,
    failed,
};

struct typed_source
{
    typed_source(source_answer source_result, std::filesystem::path presentation, std::optional<std::filesystem::path> source_root = std::nullopt,
                 std::optional<std::filesystem::path> source_relative = std::nullopt)
            : answer(source_result)
            , path(std::move(presentation))
            , root(std::move(source_root))
            , relative(std::move(source_relative))
            , descriptor(meios::source_kind::memory, false)
    {
    }

    meios::capability_descriptor capabilities() const
    {
        return descriptor;
    }

    std::optional<meios::resolved_asset> locate(std::string_view package, std::string_view relative_path) const
    {
        meios::source_lookup_result hit = try_locate(package, relative_path);
        return hit ? std::move(*hit) : std::nullopt;
    }

    meios::source_lookup_result try_locate(std::string_view, std::string_view) const
    {
        if(answer == source_answer::found && root && relative)
            return std::optional{meios::resolved_asset{path, *root, *relative}};
        if(answer == source_answer::found)
            return std::optional{meios::resolved_asset{path}};
        if(answer == source_answer::failed)
            return meios::unexpected<meios::operation_failure>({meios::operation_kind::canonicalize, {29, category}});
        return std::optional<meios::resolved_asset>{};
    }

    source_answer answer;
    std::filesystem::path path;
    std::optional<std::filesystem::path> root;
    std::optional<std::filesystem::path> relative;
    meios::capability_descriptor descriptor;
};

inline meios::scratch_dir fresh_scratch()
{
    std::error_code error;
    const std::filesystem::path parent = std::filesystem::temp_directory_path(error);
    REQUIRE_FALSE(error);
    const std::optional<std::filesystem::path> root = meios::detail::create_scratch_root(parent, error);
    REQUIRE(root.has_value());
    return meios::scratch_dir{*root};
}

class tree_guard
{
public:
    tree_guard()
            : m_tree(fresh_scratch())
    {
        std::filesystem::create_directories(path() / "docs");
        std::filesystem::create_directories(path() / "pkg");
        std::ofstream(path() / "docs" / "cfg.yaml") << "value: 7";
        std::ofstream(path() / "pkg" / "cfg.yaml") << "value: 7";
    }

    const std::filesystem::path &path() const noexcept
    {
        return m_tree.path();
    }

private:
    meios::scratch_dir m_tree;
};

inline void require_source_authorization(tree_guard &tree)
{
    meios::log_sink silent;
    meios::capturing_log_sink capture{silent};
    meios::directory_source directory{tree.path(), capture};
    const std::optional<meios::resolved_asset> directory_hit = directory.locate("pkg", "cfg.yaml");
    REQUIRE(directory_hit->source_root() == tree.path());
    REQUIRE(directory_hit->source_relative() == std::filesystem::path("pkg/cfg.yaml"));
    meios::source_stack directory_sources{directory};
    const std::vector<std::filesystem::path> roots;
    auto directory_loader = meios::detail::make_yaml_text_loader(directory_sources, roots, capture);
    REQUIRE(directory_loader("package://pkg/cfg.yaml", {}) == std::optional<std::string>{"value: 7"});

    meios::memory_source memory{capture};
    memory.add("pkg", "cfg.yaml", "value: 9");
    const std::optional<meios::resolved_asset> memory_hit = memory.locate("pkg", "cfg.yaml");
    REQUIRE(memory_hit->source_root().has_value());
    REQUIRE(memory_hit->source_relative() == std::filesystem::path("pkg/cfg.yaml"));
    meios::source_stack memory_sources{std::move(memory)};
    auto memory_loader = meios::detail::make_yaml_text_loader(memory_sources, roots, capture);
    REQUIRE(memory_loader("package://pkg/cfg.yaml", {}) == std::optional<std::string>{"value: 9"});
    REQUIRE(capture.size() == 0);
}

inline void require_read_failure(tree_guard &tree, reader_script script, meios::operation_kind operation, int value)
{
    scripted_operations operations{script};
    meios::log_sink silent;
    meios::capturing_log_sink capture{silent};
    meios::source_stack sources;
    const std::vector<std::filesystem::path> roots;
    delivery_probe delivery;
    downstream_probe downstream;
    auto loader     = meios::detail::make_yaml_text_loader(sources, roots, capture, operations, delivery);
    const auto text = loader("cfg.yaml", tree.path() / "docs" / "robot.xacro");
    downstream.consume(text);

    REQUIRE_FALSE(text.has_value());
    REQUIRE(capture.errors() == 1);
    REQUIRE(capture.first()->cause.has_value());
    REQUIRE(capture.first()->cause->operation == operation);
    REQUIRE(capture.first()->cause->native.value() == value);
    if(value != 0)
        REQUIRE(&capture.first()->cause->native.category() == &category);
    REQUIRE(delivery.count == 0);
    REQUIRE(downstream.evaluations == 0);
    REQUIRE(downstream.mutations == 0);
    REQUIRE(downstream.sentinel == "unchanged");
}

}

#endif
