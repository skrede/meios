#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>
#include <meios/diagnostic/source_location.h>
#include <meios/diagnostic/operation_failure.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

namespace
{

struct error_record
{
    meios::diagnostic_code code;
    meios::source_location loc;
    std::string message;
};

struct error_recorder
{
    std::vector<error_record> &errors;

    void operator()(meios::level lvl, const std::string &message)
    {
        if(lvl == meios::level::error)
            errors.push_back({ meios::diagnostic_code::unspecified, {}, message });
    }

    void operator()(meios::level lvl, const meios::source_location &loc, const std::string &message)
    {
        if(lvl == meios::level::error)
            errors.push_back({ meios::diagnostic_code::unspecified, loc, message });
    }

    void operator()(meios::level lvl, meios::diagnostic_code code, const meios::source_location &loc,
                    const std::string &message)
    {
        if(lvl == meios::level::error)
            errors.push_back({ code, loc, message });
    }

    void operator()(meios::level lvl, meios::diagnostic_code code, const meios::source_location &loc,
                    const meios::operation_failure &, const std::string &message)
    {
        if(lvl == meios::level::error)
            errors.push_back({ code, loc, message });
    }
};

std::vector<error_record> errors_of(const std::string &body)
{
    std::vector<error_record> errors;
    meios::log_sink_f log{ error_recorder{ errors } };
    meios::source_stack sources{};
    meios::eval_scope scope;
    const std::string source = "<robot name=\"r\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">\n" + body + "</robot>";
    REQUIRE_FALSE(meios::expand(source, scope, sources, "inline.xacro", meios::expansion_limits{}, log).has_value());
    return errors;
}

}

TEST_CASE("a deferred text that refuses gives one record, inside the property's own value", "[xacro][deferred]")
{
    const std::vector<error_record> errors = errors_of("<xacro:property name=\"q\" value=\"1\"/>\n"
                                                       "<xacro:property name=\"p\" value=\"a${nope}\"/>\n"
                                                       "<link name=\"${p}\"/>\n");
    REQUIRE(errors.size() == 1);
    CHECK(errors.front().code == meios::diagnostic_code::undefined_property);
    CHECK(errors.front().loc.line == 3);
    CHECK(errors.front().message.find("'nope'") != std::string::npos);
}

TEST_CASE("a refused deferred text read twice still gives one record", "[xacro][deferred]")
{
    const std::vector<error_record> errors = errors_of("<xacro:property name=\"p\" value=\"a${nope}\"/>\n"
                                                       "<link name=\"${p + p}\"/>\n");
    REQUIRE(errors.size() == 1);
    CHECK(errors.front().code == meios::diagnostic_code::undefined_property);
    CHECK(errors.front().loc.line == 2);
}

TEST_CASE("each read site passes a refused deferred text on without a record of its own", "[xacro][deferred]")
{
    const char *const reads[] = { "<link name=\"${p}\"/>\n", "<link name=\"x${p}\"/>\n",
                                  "<link name=\"${p.upper()}\"/>\n", "<link name=\"x${p * 2}\"/>\n" };
    for(const char *read : reads)
    {
        INFO(read);
        const std::vector<error_record> errors =
            errors_of("<xacro:property name=\"p\" value=\"a${nope}\"/>\n" + std::string(read));
        REQUIRE(errors.size() == 1);
        CHECK(errors.front().code == meios::diagnostic_code::undefined_property);
    }
}

TEST_CASE("a circular definition gives one record", "[xacro][deferred]")
{
    const std::vector<error_record> errors = errors_of("<xacro:property name=\"a\" value=\"${b}\"/>\n"
                                                       "<xacro:property name=\"b\" value=\"${a}\"/>\n"
                                                       "<link name=\"l${a}\"/>\n");
    REQUIRE(errors.size() == 1);
    CHECK(errors.front().code == meios::diagnostic_code::xacro_structural_error);
    CHECK(errors.front().message.rfind("circular variable definition: ", 0) == 0);
}

TEST_CASE("a caret default forwarding a refused deferred text gives the inner record alone", "[xacro][deferred]")
{
    const std::vector<error_record> errors =
        errors_of("<xacro:property name=\"p\" value=\"a${nope}\"/>\n"
                  "<xacro:macro name=\"m\" params=\"p:=^\"><link name=\"${p}\"/></xacro:macro>\n"
                  "<xacro:m/>\n");
    REQUIRE(errors.size() == 1);
    CHECK(errors.front().code == meios::diagnostic_code::undefined_property);
    CHECK(errors.front().loc.line == 2);
}
