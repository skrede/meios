#include "native_yaml_seam_fixture.h"

#include <meios/io/text_reader.h>
#include <meios/io/source_stack.h>
#include <meios/io/memory_source.h>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <vector>
#include <cstddef>
#include <utility>
#include <optional>
#include <filesystem>
#include <string_view>

using namespace yaml_seam_test;

TEST_CASE("an unbounded read accepts the whole file before anything charges it", "[native][yaml]")
{
    std::size_t delivered = 0;
    generating_operations operations(generated_size, delivered);

    const meios::text_read_result text = meios::detail::read_text_file("generated", operations);

    REQUIRE(text.has_value());
    CHECK(text->size() == generated_size);
    CHECK(delivered == generated_size);
}

TEST_CASE("a bounded read stops at its maximum instead of growing to the file", "[native][yaml]")
{
    std::size_t delivered = 0;
    generating_operations operations(generated_size, delivered);

    const meios::text_read_result text = meios::detail::read_text_file("generated", operations, read_ceiling);

    REQUIRE_FALSE(text.has_value());
    CHECK(text.error().kind == meios::text_read_failure_kind::too_large);
    CHECK(delivered < generated_size);
}

TEST_CASE("the auxiliary-document loader carries a byte ceiling into the read", "[native][yaml]")
{
    std::size_t delivered = 0;
    generating_operations operations(generated_size, delivered);
    recording_sink heard;
    meios::memory_source memory{ heard };
    memory.add("pkg", "cfg.yaml", "value: 9");
    meios::source_stack sources{ std::move(memory) };
    const std::vector<std::filesystem::path> roots;
    null_probe probe;
    const meios::text_resource_loader loader = meios::detail::make_yaml_text_loader(sources, roots, heard, operations, probe, read_ceiling);

    CHECK_FALSE(loader("package://pkg/cfg.yaml", {}).has_value());
    CHECK(delivered < generated_size);
    REQUIRE(heard.messages.size() == 1);
    CHECK(heard.messages.front().find("byte ceiling") != std::string::npos);
}

#ifdef MEIOS_TEST_HAS_YAML

TEST_CASE("a diagnostic sink that throws does not escape the module", "[native][yaml]")
{
    const std::shared_ptr<const meios::yaml_parser_handle> parser = meios::make_yaml_parser();
    for(bool standard : { true, false })
    {
        INFO("raises a standard exception type [" << standard << ']');
        throwing_sink sink(standard);
        meios::evaluator_counters counters;
        const meios::evaluator_limits ceilings;
        const meios::yaml_outcome out =
            (*parser)("a: !degrees pi/2\n", ceilings, counters, sink, {});
        CHECK_FALSE(out.parsed.has_value());
        CHECK(out.failure == meios::yaml_failure::unsupported);
    }
}

TEST_CASE("a scalar a fuller backend reads is retained where leniency was asked for", "[native][yaml]")
{
    const std::string_view spellings[] = { "a: .inf\n", "a: -.inf\n", "a: .nan\n", "a: 1.0e+400\n", "a: 99999999999999999999\n" };
    for(std::string_view document : spellings)
    {
        INFO("document [" << document << ']');
        for(meios::eval_policy policy : { meios::eval_policy::warn, meios::eval_policy::skip })
        {
            const std::optional<std::string> retained = substitute(document, policy);
            REQUIRE(retained.has_value());
            CHECK(retained->find("xacro.load_yaml") != std::string::npos);
        }
        CHECK_FALSE(substitute(document, meios::eval_policy::fail).has_value());
    }
}

TEST_CASE("a malformed document stays terminal under every policy", "[native][yaml]")
{
    for(meios::eval_policy policy : { meios::eval_policy::fail, meios::eval_policy::warn, meios::eval_policy::skip })
        CHECK_FALSE(substitute("a: *nope\n", policy).has_value());
}

#endif
