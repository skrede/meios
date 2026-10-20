#include <meios/xacro.h>

#include <meios/io/source_stack.h>
#include <meios/io/memory_source.h>

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>

namespace
{

using expansion_result = meios::expected<meios::expansion, meios::expansion_error>;

const char *head = R"(<robot name="p" xmlns:xacro="http://www.ros.org/wiki/xacro">)";

expansion_result expand_doc(const std::string &body, meios::log_sink &log)
{
    meios::source_stack sources{};
    meios::eval_scope scope;
    return meios::expand(head + body + "</robot>", scope, sources, "inline.xacro", meios::expansion_limits{}, log);
}

const char *one_parameter = R"(<xacro:macro name="m" params="a"><link name="${a}"/></xacro:macro>)";

expansion_result expand_calling_include(const std::string &body, meios::log_sink &log, const std::string &macro_file = one_parameter)
{
    meios::memory_source parts{ log };
    parts.add("pkg", "m.xacro", R"(<robot xmlns:xacro="http://www.ros.org/wiki/xacro">)" + macro_file + "</robot>");
    meios::source_stack sources{ std::move(parts) };
    meios::eval_scope scope;
    return meios::expand(head + body + "</robot>", scope, sources, "top.xacro", meios::expansion_limits{}, log);
}

void require_refused_at(const expansion_result &out, const char *file, int line)
{
    REQUIRE_FALSE(out.has_value());
    REQUIRE(out.error().code == meios::diagnostic_code::xacro_structural_error);
    REQUIRE(out.error().loc.file.filename() == file);
    REQUIRE(out.error().loc.line == line);
}

const char *one_block = R"(<xacro:macro name="m" params="a *blk"><link name="${a}"/></xacro:macro>)";

void require_refusal(const expansion_result &out, meios::diagnostic_code code, const char *phrase)
{
    REQUIRE_FALSE(out.has_value());
    REQUIRE(out.error().code == code);
    REQUIRE(out.error().message.find(phrase) != std::string::npos);
}

const char *child_property = R"(<xacro:property name="p" value="from_child"/>)";

const meios::diagnostic_code structural = meios::diagnostic_code::xacro_structural_error;
const meios::diagnostic_code undefined = meios::diagnostic_code::undefined_property;

}

TEST_CASE("a call attribute naming no declared parameter refuses at the call, naming the parameter and the macro",
          "[xacro][macro][call]")
{
    meios::log_sink silent;
    const expansion_result out = expand_doc(std::string(one_parameter) + "\n\n<xacro:m a=\"x\" b=\"y\"/>", silent);
    require_refused_at(out, "inline.xacro", 3);
    REQUIRE(out.error().message.find("Invalid parameter \"b\"") != std::string::npos);
    REQUIRE(out.error().message.find("'m'") != std::string::npos);
}

TEST_CASE("a call attribute spelled like a block parameter, or declaring a namespace, refuses as undeclared",
          "[xacro][macro][call]")
{
    meios::log_sink silent;
    const expansion_result block_named =
        expand_doc(R"(<xacro:macro name="m" params="*blk"><xacro:insert_block name="blk"/></xacro:macro>)"
                   R"(<xacro:m blk="x"><link name="l"/></xacro:m>)",
                   silent);
    REQUIRE_FALSE(block_named.has_value());
    REQUIRE(block_named.error().code == meios::diagnostic_code::xacro_structural_error);
    REQUIRE(block_named.error().message.find("Invalid parameter \"blk\"") != std::string::npos);

    const expansion_result declaring =
        expand_doc(std::string(one_parameter) + R"(<xacro:m a="x" xmlns:foo="urn:foo"/>)", silent);
    REQUIRE_FALSE(declaring.has_value());
    REQUIRE(declaring.error().code == meios::diagnostic_code::xacro_structural_error);
    REQUIRE(declaring.error().message.find("Invalid parameter \"xmlns:foo\"") != std::string::npos);
}

TEST_CASE("a call to a macro defined in an included file refuses at the call's own file and line",
          "[xacro][macro][call]")
{
    meios::log_sink silent;
    const expansion_result out =
        expand_calling_include("<xacro:include filename=\"$(find pkg)/m.xacro\"/>\n\n\n<xacro:m a=\"x\" b=\"y\"/>",
                               silent);
    require_refused_at(out, "top.xacro", 4);
    REQUIRE(out.error().message.find("Invalid parameter \"b\"") != std::string::npos);
}

TEST_CASE("a missing parameter of a macro defined in an included file is reported at the call",
          "[xacro][macro][call]")
{
    meios::log_sink silent;
    const expansion_result out =
        expand_calling_include("<xacro:include filename=\"$(find pkg)/m.xacro\"/>\n\n\n<xacro:m/>", silent);
    require_refused_at(out, "top.xacro", 4);
    REQUIRE(out.error().message.find("missing required parameter 'a'") != std::string::npos);
}

TEST_CASE("a default that refuses while the call supplied its parameter is reported at the call, for a macro defined "
          "in another file",
          "[xacro][macro][call]")
{
    meios::log_sink silent;
    const expansion_result out = expand_calling_include("<xacro:include filename=\"$(find pkg)/m.xacro\"/>\n\n\n\n\n\n\n\n<xacro:m a=\"x\"/>", silent,
                                                        "\n\n\n<xacro:macro name=\"m\" params=\"a:=${nope}\"><link name=\"${a}\"/></xacro:macro>");
    REQUIRE_FALSE(out.has_value());
    REQUIRE(out.error().code == meios::diagnostic_code::undefined_property);
    REQUIRE(out.error().loc.file.filename() == "top.xacro");
    REQUIRE(out.error().loc.line == 9);
}

TEST_CASE("elements a call passes beyond the macro's block parameters refuse, naming the first left over",
          "[xacro][macro][call]")
{
    meios::log_sink silent;
    require_refusal(expand_doc(std::string(one_parameter) + R"(<xacro:m a="x"><first/><second/></xacro:m>)", silent),
                    structural, "Unused block \"first\"");
}

TEST_CASE("a child that expands to nothing is not a block", "[xacro][macro][call]")
{
    meios::log_sink silent;
    REQUIRE(expand_doc(std::string(one_parameter) + R"(<xacro:m a="x"><xacro:property name="p" value="1"/></xacro:m>)", silent)
                .has_value());
}

TEST_CASE("an undeclared attribute is reported before a missing parameter", "[xacro][macro][call]")
{
    meios::log_sink silent;
    require_refusal(expand_doc(std::string(one_parameter) + R"(<xacro:m b="y"/>)", silent), structural, "Invalid parameter \"b\"");
}

TEST_CASE("an undeclared attribute is reported before a block child that refuses", "[xacro][macro][call]")
{
    meios::log_sink silent;
    require_refusal(expand_doc(std::string(one_block) + R"(<xacro:m a="x" b="y"><link name="${nope}"/></xacro:m>)", silent),
                    structural, "Invalid parameter \"b\"");
}

TEST_CASE("a surplus child is reported before a missing parameter", "[xacro][macro][call]")
{
    meios::log_sink silent;
    require_refusal(expand_doc(std::string(one_parameter) + R"(<xacro:m><visual/></xacro:m>)", silent), structural,
                    "Unused block \"visual\"");
}

TEST_CASE("call attributes refuse in document order", "[xacro][macro][call]")
{
    meios::log_sink silent;
    const std::string bound = one_parameter;
    require_refusal(expand_doc(bound + R"(<xacro:m b="y" a="${nope}"/>)", silent), structural, "Invalid parameter \"b\"");
    require_refusal(expand_doc(bound + R"(<xacro:m a="${nope}" b="y"/>)", silent), undefined, "'nope'");
}

TEST_CASE("a call's attributes do not see a property the call's own child defines", "[xacro][macro][call]")
{
    meios::log_sink silent;
    require_refusal(expand_doc(std::string(one_parameter) + R"(<xacro:m a="${p}">)" + child_property + "</xacro:m>", silent),
                    undefined, "'p'");
}

TEST_CASE("a default sees a property the call's own child defines", "[xacro][macro][call]")
{
    meios::log_sink silent;
    const expansion_result out =
        expand_doc(R"(<xacro:macro name="m" params="a:=${p}"><link name="${a}"/></xacro:macro><xacro:m>)"
                       + std::string(child_property) + "</xacro:m>",
                   silent);
    REQUIRE(out.has_value());
    REQUIRE(out->document.find("name=\"from_child\"") != std::string::npos);
}
