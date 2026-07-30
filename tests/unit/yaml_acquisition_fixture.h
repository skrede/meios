#ifndef HPP_GUARD_MEIOS_TEST_YAML_ACQUISITION_FIXTURE_H
#define HPP_GUARD_MEIOS_TEST_YAML_ACQUISITION_FIXTURE_H

#include "acquisition_script.h"
#include "meios/urdf/yaml_resource.h"

#include <meios/io/source_stack.h>
#include <meios/io/source_lookup.h>

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
    void delivered() override
    {
        ++count;
    }

    int count{0};
};

struct downstream_probe
{
    void consume(const std::optional<std::string> &text)
    {
        if(!text)
            return;
        ++yaml_parses;
        ++evaluations;
        ++mutations;
        sentinel = *text;
    }

    int yaml_parses{0};
    int evaluations{0};
    int mutations{0};
    std::string sentinel{"unchanged"};
};

enum class source_answer
{
    absent,
    found,
    failed,
};

struct typed_source
{
    meios::capability_descriptor capabilities() const
    {
        return descriptor;
    }

    std::optional<meios::resolved_asset> locate(std::string_view package, std::string_view relative) const
    {
        meios::source_lookup_result hit = try_locate(package, relative);
        return hit ? std::move(*hit) : std::nullopt;
    }

    meios::source_lookup_result try_locate(std::string_view, std::string_view) const
    {
        if(answer == source_answer::found)
            return std::optional{meios::resolved_asset{path}};
        if(answer == source_answer::failed)
            return meios::unexpected<meios::operation_failure>({meios::operation_kind::canonicalize, {29, category}});
        return std::optional<meios::resolved_asset>{};
    }

    source_answer answer;
    std::filesystem::path path;
    meios::capability_descriptor descriptor{meios::source_kind::memory, false};
};

class tree_guard
{
public:
    tree_guard()
            : root(std::filesystem::temp_directory_path() / "meios-yaml-acquisition")
    {
        std::error_code error;
        std::filesystem::remove_all(root, error);
        std::filesystem::create_directories(root / "docs");
        std::ofstream(root / "docs" / "cfg.yaml") << "value: 7";
    }

    ~tree_guard()
    {
        std::error_code error;
        std::filesystem::remove_all(root, error);
    }

    std::filesystem::path root;
};

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
    const auto text = loader("cfg.yaml", tree.root / "docs" / "robot.xacro");
    downstream.consume(text);

    REQUIRE_FALSE(text.has_value());
    REQUIRE(capture.errors() == 1);
    REQUIRE(capture.first()->cause.has_value());
    REQUIRE(capture.first()->cause->operation == operation);
    REQUIRE(capture.first()->cause->native.value() == value);
    if(value != 0)
        REQUIRE(&capture.first()->cause->native.category() == &category);
    REQUIRE(delivery.count == 0);
    REQUIRE(downstream.yaml_parses == 0);
    REQUIRE(downstream.evaluations == 0);
    REQUIRE(downstream.mutations == 0);
    REQUIRE(downstream.sentinel == "unchanged");
}

}

#endif
