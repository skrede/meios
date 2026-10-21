#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>

#include <rapidcheck.h>
#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstddef>

namespace
{

const std::string head = "<robot name=\"p\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">\n"
                         "<xacro:property name=\"x\" value=\"vx\"/><xacro:arg name=\"y\" default=\"Y\"/>\n";

const std::vector<std::string> atoms = {
    "${[1, 2]}", "${x}", "${nope}", "$(arg y)", "$(arg nope)", "$(arg $(x))", "$(arg ${x)", "${'$(arg $(x))'}",
    "$$x", "$${x}", "${x", "$(x", "$", "a", " ", "}", ")",
};

std::string generated_name()
{
    const std::size_t count = *rc::gen::inRange<std::size_t>(1, 6);
    const std::vector<std::string> parts = *rc::gen::container<std::vector<std::string>>(count, rc::gen::elementOf(atoms));
    std::string name;
    for(const std::string &part : parts)
        name += part;
    return name;
}

bool returns_beside_an_error(const std::string &name, meios::eval_policy policy)
{
    std::size_t errors = 0;
    meios::log_sink_f log{ [&errors](meios::level lvl, auto &&...) {
        if(lvl == meios::level::error)
            ++errors;
    } };
    meios::source_stack sources{};
    meios::eval_scope scope;
    const auto out = meios::expand(head + "<link name=\"" + name + "\"/></robot>", scope, sources, "inline.xacro",
                                   meios::expansion_limits{}, policy, {}, log);
    return out.has_value() && errors != 0;
}

}

TEST_CASE("under a lenient policy an expansion that logs an error never returns a document", "[xacro][property]")
{
    REQUIRE(rc::check("a returned document carries no error-level record", [] {
        const std::string name = generated_name();
        RC_ASSERT_FALSE(returns_beside_an_error(name, meios::eval_policy::warn));
        RC_ASSERT_FALSE(returns_beside_an_error(name, meios::eval_policy::skip));
    }));
}
