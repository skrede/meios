#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#include <meios/diagnostic/log_sink.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <fstream>
#include <iterator>
#include <filesystem>

namespace
{

using expansion_result = meios::expected<meios::expansion, meios::expansion_error>;

// One macro of one parameter, emitting that parameter: the two things every case here varies
// are the parameter spelling and what the call supplies for it.
expansion_result call_with(const std::string &spec, const std::string &call, meios::log_sink &log)
{
    meios::source_stack sources{};
    meios::eval_scope scope;
    const std::string source =
        R"(<robot name="r" xmlns:xacro="http://www.ros.org/wiki/xacro">)"
        R"(<xacro:macro name="m" params=")" + spec
      + R"("><link name="${p}"/></xacro:macro><xacro:m )" + call + "/></robot>";
    return meios::expand(source, scope, sources, "inline.xacro", meios::expansion_limits{}, log);
}

// The minimized document a behavior was measured on against the reference, read from where the
// differential left it once the two sides agreed and it named no divergence.
expansion_result expand_probe(const char *name, meios::log_sink &log)
{
    std::ifstream in(std::filesystem::path{ MEIOS_XACRO_PROBE_DIR } / name, std::ios::binary);
    std::string source;
    source.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    meios::source_stack sources{};
    meios::eval_scope scope;
    return meios::expand(source, scope, sources, "probe.xacro", meios::expansion_limits{}, log);
}

}

TEST_CASE("an overridden default naming an unbound property refuses the load",
          "[xacro][structural][macro]")
{
    meios::log_sink silent;
    const expansion_result out = call_with("p:=${nobody_bound_here}", R"(p="given")", silent);
    REQUIRE_FALSE(out.has_value());
    REQUIRE(out.error().message.find("nobody_bound_here") != std::string::npos);
}

TEST_CASE("an overridden default is discarded and the caller's value is what binds",
          "[xacro][structural][macro]")
{
    meios::log_sink silent;
    const expansion_result out = call_with("p:=from_default", R"(p="given")", silent);
    REQUIRE(out.has_value());
    REQUIRE(out->document.find(R"(name="given")") != std::string::npos);
    REQUIRE(out->document.find("from_default") == std::string::npos);
}

// The reference substitutes the parameter spelling as text, so neither inheritance arm is
// interpreted there and neither refuses; a bare caret carrying nothing to inherit renders on
// both sides, measured against the pinned reference.
TEST_CASE("an overridden inheriting default is no error merely because it is evaluated",
          "[xacro][structural][macro]")
{
    meios::log_sink silent;
    const expansion_result bare = call_with("p:=^", R"(p="given")", silent);
    const expansion_result fallback = call_with("p:=^|fb", R"(p="given")", silent);
    REQUIRE(bare.has_value());
    REQUIRE(bare->document.find(R"(name="given")") != std::string::npos);
    REQUIRE(fallback.has_value());
    REQUIRE(fallback->document.find(R"(name="given")") != std::string::npos);
}

TEST_CASE("a construct written only inside an overridden default reaches the observed set",
          "[xacro][structural][macro]")
{
    meios::log_sink silent;
    const expansion_result out = call_with("p:=${dict(a=1)}", R"(p="given")", silent);
    REQUIRE(out.has_value());
    REQUIRE(out->exercised.holds(meios::evaluator_construct::mapping_literal));
}

// An evaluated default charges the load's own evaluator ceilings because it travels the
// substitution path every other attribute travels rather than a second one of its own.
TEST_CASE("an overridden default crossing an evaluator ceiling refuses rather than running",
          "[xacro][structural][macro]")
{
    meios::log_sink silent;
    const expansion_result out = call_with("p:=${1000000000001**1}", R"(p="given")", silent);
    REQUIRE_FALSE(out.has_value());
    REQUIRE(out.error().message.find("numeric magnitude") != std::string::npos);
}

TEST_CASE("a parameter carrying no default still binds a supplied attribute",
          "[xacro][structural][macro]")
{
    meios::log_sink silent;
    const expansion_result out = call_with("p", R"(p="given")", silent);
    REQUIRE(out.has_value());
    REQUIRE(out->document.find(R"(name="given")") != std::string::npos);
}

TEST_CASE("the minimized overridden-default document refuses in the reference's own words",
          "[xacro][structural][macro]")
{
    meios::log_sink silent;
    const expansion_result out = expand_probe("overridden_default.xacro", silent);
    REQUIRE_FALSE(out.has_value());
    REQUIRE(out.error().message == "name 'nobody_bound_here' is not defined");
}
