#include "xacro_include_tree.h"

#include <meios/xacro.h>

#include <meios/io/source_stack.h>
#include <meios/io/directory_source.h>

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>

namespace
{

using expansion_result = meios::expected<meios::expansion, meios::expansion_error>;

const std::string head = "<robot name=\"p\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">\n"
                         "<xacro:property name=\"x\" value=\"vx\"/><xacro:arg name=\"y\" default=\"Y\"/>\n";

expansion_result expand_doc(const std::string &body, meios::eval_policy policy = meios::eval_policy::fail)
{
    meios::log_sink_f log{ [](auto &&...) {} };
    meios::source_stack sources{};
    meios::eval_scope scope;
    return meios::expand(head + body + "</robot>", scope, sources, "inline.xacro",
                         meios::expansion_limits{}, policy, {}, log);
}

expansion_result expand_name(const std::string &name, meios::eval_policy policy = meios::eval_policy::fail)
{
    return expand_doc("<link name=\"" + name + "\"/>", policy);
}

expansion_result expand_including(const std::string &filename)
{
    meios::log_sink_f log{ [](auto &&...) {} };
    const include_tree::tree held;
    held.write("root/p/urdf/a.xacro", include_tree::robot("<link name=\"from_package\"/>"));
    meios::source_stack sources{ meios::directory_source{ held.root() / "root", log } };
    meios::eval_scope scope;
    const std::string body = "<xacro:property name=\"pk\" value=\"p\"/><xacro:include filename=\"" + filename + "\"/>";
    return meios::expand(head + body + "</robot>", scope, sources, held.root() / "doc" / "top.xacro",
                         meios::expansion_limits{}, log);
}

void require_refused(const expansion_result &out, const std::string &message)
{
    REQUIRE_FALSE(out.has_value());
    CHECK(out.error().code == meios::diagnostic_code::expression_error);
    CHECK(out.error().message.starts_with(message));
}

}

TEST_CASE("a run of dollars before a brace or a parenthesis emits one dollar fewer and the opener",
          "[xacro][dollar_run]")
{
    const std::pair<const char *, const char *> table[] = {
        { "$${x}", "${x}" },         { "$$(arg y)", "$(arg y)" }, { "$$$${x}", "$$${x}" },
        { "$$$(arg y)", "$$(arg y)" }, { "$${${x}}", "${vx}" },  { "$x$${x}", "$x${x}" },
        { "$${x", "${x" },           { "$${", "${" },
    };
    for(const auto &[raw, rendered] : table)
    {
        INFO("raw: " << raw);
        const expansion_result out = expand_name(raw);
        REQUIRE(out.has_value());
        CHECK(out->document.find("name=\"" + std::string(rendered) + "\"") != std::string::npos);
    }
}

TEST_CASE("a double dollar before anything else refuses with the reference's wording",
          "[xacro][dollar_run]")
{
    for(const char *raw : { "$$x", "a$$", "$$$x", "${x}$$", "$(arg y)$$" })
    {
        INFO("raw: " << raw);
        require_refused(expand_name(raw), "invalid expression: $$");
    }
    CHECK(expand_name("$$$x").error().message == "invalid expression: $$$x");
}

TEST_CASE("a single dollar before anything but an opener stays literal", "[xacro][dollar_run]")
{
    for(const char *raw : { "$x", "a$", "$", "p$q$", "$ {x}" })
    {
        INFO("raw: " << raw);
        const expansion_result out = expand_name(raw);
        REQUIRE(out.has_value());
        CHECK(out->document.find("name=\"" + std::string(raw) + "\"") != std::string::npos);
    }
}

TEST_CASE("a double dollar inside an expression body refuses as the reference lexes the body",
          "[xacro][dollar_run]")
{
    const expansion_result out = expand_name("${'$$'}");
    require_refused(out, "invalid expression: $$'");
}

TEST_CASE("a double dollar in a text node refuses", "[xacro][dollar_run]")
{
    require_refused(expand_doc("<link name=\"l\"><t>a $$x</t></link>"), "invalid expression: $$x");
}

TEST_CASE("a refused double dollar is terminal under a lenient policy", "[xacro][dollar_run]")
{
    for(meios::eval_policy policy : { meios::eval_policy::warn, meios::eval_policy::skip })
    {
        require_refused(expand_name("$$x", policy), "invalid expression: $$x");
        require_refused(expand_name("${'$$'}", policy), "invalid expression: $$'");
    }
}

// The link sits on line 3, and its value's run begins at 1-based column 16.
TEST_CASE("the refusal is located at the run", "[xacro][dollar_run]")
{
    const expansion_result out = expand_name("abc$$x");
    REQUIRE_FALSE(out.has_value());
    CHECK(out.error().loc.line == 3);
    CHECK(out.error().loc.column == 16);
}

TEST_CASE("an include filename is a package lookup only where it begins with a find", "[xacro][dollar_run][include]")
{
    for(const char *filename : { "$$(find p)/urdf/a.xacro", "sub/$(find p)/urdf/a.xacro" })
    {
        INFO("filename: " << filename);
        const expansion_result out = expand_including(filename);
        REQUIRE_FALSE(out.has_value());
        CHECK(out.error().code == meios::diagnostic_code::unresolved_include);
    }
    for(const char *filename : { "$(find p)/urdf/a.xacro", "package://p/urdf/a.xacro" })
    {
        INFO("filename: " << filename);
        const expansion_result out = expand_including(filename);
        REQUIRE(out.has_value());
        CHECK(out->document.find("from_package") != std::string::npos);
    }
}

TEST_CASE("the package text of a leading find is substituted", "[xacro][dollar_run][include]")
{
    const expansion_result out = expand_including("$(find ${pk})/urdf/a.xacro");
    REQUIRE(out.has_value());
    CHECK(out->document.find("from_package") != std::string::npos);
}

TEST_CASE("the package text of a package url is left as written", "[xacro][dollar_run][include]")
{
    const expansion_result out = expand_including("package://${pk}/urdf/a.xacro");
    REQUIRE_FALSE(out.has_value());
    CHECK(out.error().code == meios::diagnostic_code::unresolved_include);
    CHECK(out.error().message.find("${pk}") != std::string::npos);
}
