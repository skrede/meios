#include "xacro_expand_fixture.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <filesystem>


TEST_CASE("every structural golden expands to its expected URDF", "[xacro][structural][golden]")
{
    const std::filesystem::path root = std::filesystem::path(MEIOS_GOLDEN_DIR) / "structural";
    meios::log_sink silent;
    int cases = 0;
    for(const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(root))
    {
        if(!entry.is_directory() || !std::filesystem::exists(entry.path() / "input.xacro"))
            continue;
        INFO("case: " << entry.path().filename().string());
        std::string expected = meios::canonical_xml(read_file(entry.path() / "expected.urdf"));
        REQUIRE(expand_case(entry.path(), silent) == expected);
        ++cases;
    }
    REQUIRE(cases == 3);
}

TEST_CASE("a caret macro default inherits the enclosing binding", "[xacro][structural][macro]")
{
    meios::log_sink silent;
    const expansion_result out = expand_source(inherit_hit, silent);
    REQUIRE(out.has_value());
    REQUIRE(out->document.find("l7") != std::string::npos);
}

TEST_CASE("a caret-fallback macro default resolves the fallback when nothing is inherited",
          "[xacro][structural][macro]")
{
    meios::log_sink silent;
    const expansion_result out = expand_source(inherit_fallback, silent);
    REQUIRE(out.has_value());
    REQUIRE(out->document.find("l5") != std::string::npos);
}

TEST_CASE("a caret macro default with no inherited value and no fallback fails loudly",
          "[xacro][structural][macro]")
{
    meios::log_sink silent;
    const expansion_result out = expand_source(inherit_missing, silent);
    REQUIRE_FALSE(out.has_value());
    REQUIRE(out.error().code == meios::diagnostic_code::xacro_structural_error);
}

TEST_CASE("a scope=parent property reaches the immediate caller but not the grandparent",
          "[xacro][structural][property]")
{
    meios::log_sink silent;
    const expansion_result out = expand_source(parent_one_level, silent);
    REQUIRE(out.has_value());
    REQUIRE(out->document.find("in9") != std::string::npos);
    REQUIRE(out->document.find("top1") != std::string::npos);
}

TEST_CASE("a scope=parent property set at top level persists into the document scope",
          "[xacro][structural][property]")
{
    meios::log_sink silent;
    const expansion_result out = expand_source(parent_at_top, silent);
    REQUIRE(out.has_value());
    REQUIRE(out->document.find("g5") != std::string::npos);
}

TEST_CASE("a default-scope property set inside a macro is macro-local and invisible after return",
          "[xacro][structural][property]")
{
    meios::log_sink silent;
    const expansion_result out = expand_source(default_local, silent);
    REQUIRE_FALSE(out.has_value());
    REQUIRE(out.error().code == meios::diagnostic_code::undefined_property);
}

TEST_CASE("a default-scope property is visible to a nested macro while the definer is on the stack",
          "[xacro][structural][property]")
{
    meios::log_sink silent;
    const expansion_result out = expand_source(nested_default_read, silent);
    REQUIRE(out.has_value());
    REQUIRE(out->document.find("j3") != std::string::npos);
}

TEST_CASE("a default-scope property read from the document after the outer macro returns fails",
          "[xacro][structural][property]")
{
    meios::log_sink silent;
    const expansion_result out = expand_source(nested_default_no_leak, silent);
    REQUIRE_FALSE(out.has_value());
    REQUIRE(out.error().code == meios::diagnostic_code::undefined_property);
}

TEST_CASE("a default-scope write to a name that is also the writer's parameter restores the outer "
          "document value on exit",
          "[xacro][structural][property]")
{
    meios::log_sink silent;
    const expansion_result out = expand_source(param_default_collision, silent);
    REQUIRE(out.has_value());
    REQUIRE(out->document.find("ldoc") != std::string::npos);
    REQUIRE(out->document.find("lbparam") == std::string::npos);
    REQUIRE(out->document.find("l99") == std::string::npos);
}

TEST_CASE("a scope=global property set inside a macro reaches the document scope",
          "[xacro][structural][property]")
{
    meios::log_sink silent;
    const expansion_result out = expand_source(global_reaches_doc, silent);
    REQUIRE(out.has_value());
    REQUIRE(out->document.find("gl7") != std::string::npos);
}

TEST_CASE("a default-scope property defined at document top level persists for a later read",
          "[xacro][structural][property]")
{
    meios::log_sink silent;
    const expansion_result out = expand_source(top_level_default, silent);
    REQUIRE(out.has_value());
    REQUIRE(out->document.find("tl4") != std::string::npos);
}

TEST_CASE("a scope=parent write to a name that is also the writer's parameter never leaks "
          "the private parameter value into the document scope",
          "[xacro][structural][property]")
{
    meios::log_sink silent;
    const expansion_result out = expand_source(param_parent_collision, silent);
    REQUIRE(out.has_value());
    REQUIRE(out->document.find("ldoc") != std::string::npos);
    REQUIRE(out->document.find("lbparam") == std::string::npos);
    REQUIRE(out->document.find("l99") == std::string::npos);
}

TEST_CASE("a scope=parent write of a parameter value under a distinct name reaches the caller",
          "[xacro][structural][property]")
{
    meios::log_sink silent;
    const expansion_result out = expand_source(parent_publish, silent);
    REQUIRE(out.has_value());
    REQUIRE(out->document.find("wabc") != std::string::npos);
}

TEST_CASE("an included top-level property is macro-local when the include is nested in a macro body",
          "[xacro][structural][property][include]")
{
    meios::log_sink silent;
    const expansion_result out = expand_with_include(include_in_macro, silent);
    REQUIRE_FALSE(out.has_value());
    REQUIRE(out.error().code == meios::diagnostic_code::undefined_property);
}

TEST_CASE("an included top-level property persists when the include sits at the document top level",
          "[xacro][structural][property][include]")
{
    meios::log_sink silent;
    const expansion_result out = expand_with_include(include_at_top, silent);
    REQUIRE(out.has_value());
    REQUIRE(out->document.find("x42") != std::string::npos);
}
