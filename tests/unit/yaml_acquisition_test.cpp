#include "yaml_acquisition_fixture.h"
#include "yaml_replacement_operations.h"

#if defined(MEIOS_TEST_HAS_EVAL_PYTHON)
    #include <meios/eval/python_evaluator.h>

    #include <meios/xacro/eval_scope.h>
#endif

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <optional>
#include <filesystem>

namespace
{

using acquisition_test::delivery_probe;
using acquisition_test::downstream_probe;
using acquisition_test::source_answer;
using acquisition_test::tree_guard;
using acquisition_test::typed_source;

}

TEST_CASE("YAML read failures stop before delivery evaluation and mutation", "[yaml][acquisition]")
{
    tree_guard tree;
    acquisition_test::reader_script status;
    status.status_error = {11, acquisition_test::category};
    acquisition_test::require_read_failure(tree, status, meios::operation_kind::status, 11);
    acquisition_test::reader_script non_regular;
    non_regular.status = std::filesystem::file_status(std::filesystem::file_type::directory);
    acquisition_test::require_read_failure(tree, non_regular, meios::operation_kind::status, 0);
    acquisition_test::reader_script open;
    open.open_error = {13, acquisition_test::category};
    acquisition_test::require_read_failure(tree, open, meios::operation_kind::open, 13);
    acquisition_test::reader_script read;
    read.read_error = {17, acquisition_test::category};
    acquisition_test::require_read_failure(tree, read, meios::operation_kind::read, 17);
    acquisition_test::reader_script close;
    close.close_error = {19, acquisition_test::category};
    acquisition_test::require_read_failure(tree, close, meios::operation_kind::close, 19);
}

TEST_CASE("path and recovered package acquisition deliver exact text once", "[yaml][acquisition]")
{
    tree_guard tree;
    acquisition_test::reader_script script;
    script.content = "value: 7";
    acquisition_test::scripted_operations operations{script};
    meios::log_sink silent;
    meios::capturing_log_sink capture{silent};
    meios::source_stack sources{typed_source{source_answer::failed, {}}, typed_source{source_answer::found, tree.path() / "docs" / "cfg.yaml", tree.path(), "docs/cfg.yaml"}};
    const std::vector<std::filesystem::path> roots;
    delivery_probe delivery;
    auto loader = meios::detail::make_yaml_text_loader(sources, roots, capture, operations, delivery);

    downstream_probe downstream;
    const auto package = loader("package://pkg/cfg.yaml", {});
    downstream.consume(package);
    REQUIRE(script.calls.front() == "open_under");
    script.calls.clear();
    script.offset   = 0;
    const auto path = loader("cfg.yaml", tree.path() / "docs" / "robot.xacro");
    downstream.consume(path);

    REQUIRE(package == std::optional<std::string>{"value: 7"});
    REQUIRE(path == package);
    REQUIRE(delivery.count == 2);
    REQUIRE(downstream.evaluations == 2);
    REQUIRE(downstream.mutations == 2);
    REQUIRE(downstream.sentinel == "value: 7");
    REQUIRE(capture.size() == 0);
}

TEST_CASE("directory and memory sources carry checked-open authorization", "[yaml][acquisition]")
{
    tree_guard tree;
    acquisition_test::require_source_authorization(tree);
    std::ofstream(tree.path() / "sentinel") << "alive";
    {
        tree_guard second;
        REQUIRE(second.path() != tree.path());
    }
    REQUIRE(std::filesystem::exists(tree.path() / "sentinel"));
}

TEST_CASE("relative roots recover from absence and native failure", "[yaml][acquisition]")
{
    tree_guard tree;
    acquisition_test::reader_script script;
    script.content         = "value: 7";
    script.next_open_error = {31, acquisition_test::category};
    acquisition_test::scripted_operations operations{script};
    meios::log_sink silent;
    meios::capturing_log_sink capture{silent};
    meios::source_stack sources;
    const std::vector<std::filesystem::path> roots{tree.path() / "missing", tree.path() / "pkg", tree.path() / "docs"};
    delivery_probe delivery;
    auto loader = meios::detail::make_yaml_text_loader(sources, roots, capture, operations, delivery);

    REQUIRE(loader("cfg.yaml", {}) == std::optional<std::string>{"value: 7"});
    REQUIRE(script.calls == std::vector<std::string>{"open_under", "open_under", "read", "error", "read", "error", "close"});
    REQUIRE(capture.size() == 0);
}

TEST_CASE("relative terminal failures and semantic escapes stay distinct", "[yaml][acquisition]")
{
    tree_guard tree;
    acquisition_test::reader_script script;
    script.open_error = {37, acquisition_test::category};
    acquisition_test::scripted_operations operations{script};
    meios::log_sink silent;
    meios::capturing_log_sink capture{silent};
    meios::source_stack sources;
    const std::vector<std::filesystem::path> roots{tree.path() / "docs"};
    delivery_probe delivery;
    auto loader = meios::detail::make_yaml_text_loader(sources, roots, capture, operations, delivery);
    REQUIRE_FALSE(loader("cfg.yaml", {}).has_value());
    REQUIRE(capture.first()->cause.has_value());
    REQUIRE(capture.first()->cause->operation == meios::operation_kind::open);
    REQUIRE(capture.first()->cause->native == script.open_error);
    meios::capturing_log_sink escape_capture{silent};
    auto escape_loader = meios::detail::make_yaml_text_loader(sources, roots, escape_capture, operations, delivery);
    REQUIRE_FALSE(escape_loader("../pkg/cfg.yaml", {}).has_value());
    REQUIRE(escape_capture.first()->code == meios::diagnostic_code::uncontained_asset);
    REQUIRE_FALSE(escape_capture.first()->cause.has_value());
}

TEST_CASE("source replacement before YAML read is rejected", "[yaml][acquisition]")
{
    tree_guard tree;
    std::ofstream(tree.path() / "outside.yaml") << "outside";
    meios::log_sink silent;
    meios::capturing_log_sink capture{silent};
    meios::source_stack sources{meios::directory_source{tree.path(), capture}};
    acquisition_test::replacement_operations operations{tree.path() / "outside.yaml"};
    const std::vector<std::filesystem::path> roots;
    delivery_probe delivery;
    auto loader = meios::detail::make_yaml_text_loader(sources, roots, capture, operations, delivery);

    REQUIRE_FALSE(loader("package://pkg/cfg.yaml", {}).has_value());
    REQUIRE(operations.replacement_created());
    REQUIRE(capture.first()->cause.has_value());
    REQUIRE(delivery.count == 0);
}

TEST_CASE("YAML all miss and native exhaustion keep distinct terminal behavior", "[yaml][acquisition]")
{
    acquisition_test::reader_script script;
    acquisition_test::scripted_operations operations{script};
    meios::log_sink silent;
    meios::capturing_log_sink absent_capture{silent};
    meios::source_stack absent{typed_source{source_answer::absent, {}}};
    const std::vector<std::filesystem::path> roots;
    delivery_probe absent_delivery;
    auto absent_loader = meios::detail::make_yaml_text_loader(absent, roots, absent_capture, operations, absent_delivery);
    REQUIRE_FALSE(absent_loader("package://pkg/cfg.yaml", {}).has_value());
    REQUIRE(absent_capture.errors() == 1);
    REQUIRE_FALSE(absent_capture.first()->cause.has_value());
    REQUIRE(absent_delivery.count == 0);

    meios::capturing_log_sink failed_capture{silent};
    meios::source_stack failed{typed_source{source_answer::failed, {}}};
    delivery_probe failed_delivery;
    auto failed_loader = meios::detail::make_yaml_text_loader(failed, roots, failed_capture, operations, failed_delivery);
    REQUIRE_FALSE(failed_loader("package://pkg/cfg.yaml", {}).has_value());
    REQUIRE(failed_capture.errors() == 1);
    REQUIRE(failed_capture.first()->cause->operation == meios::operation_kind::canonicalize);
    REQUIRE(failed_capture.first()->cause->native.value() == 29);
    REQUIRE(&failed_capture.first()->cause->native.category() == &acquisition_test::category);
    REQUIRE(failed_delivery.count == 0);
}

TEST_CASE("the Python YAML continuation runs only after successful delivery", "[yaml][acquisition]")
{
#if defined(MEIOS_TEST_HAS_EVAL_PYTHON)
    tree_guard tree;
    acquisition_test::reader_script script;
    script.content = "value: 7";
    acquisition_test::scripted_operations operations{script};
    meios::log_sink silent;
    meios::source_stack sources;
    const std::vector<std::filesystem::path> roots;
    delivery_probe delivery;
    meios::eval_scope scope;
    scope.set_active_document(tree.path() / "docs" / "robot.xacro");
    scope.install_text_loader(meios::detail::make_yaml_text_loader(sources, roots, silent, operations, delivery));
    meios::python_evaluator evaluator;
    const auto value = evaluator.eval_to_text("load_yaml('cfg.yaml')['value']", scope, silent).text;

    REQUIRE(value == std::optional<std::string>{"7"});
    REQUIRE(delivery.count == 1);
#else
    SUCCEED("the optional Python evaluator is unavailable");
#endif
}
