#include <meios/xacro.h>

#include <meios/io/source_stack.h>
#include <meios/io/memory_source.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <utility>
#include <filesystem>
#include <string_view>

namespace
{

struct captured_log
{
    std::vector<std::pair<meios::level, std::string>> &records;

    void operator()(meios::level lvl, const std::string &message)
    {
        records.push_back({ lvl, message });
    }

    void operator()(meios::level, const meios::source_location &, const std::string &) {}
};

bool any_contains(const std::vector<std::pair<meios::level, std::string>> &records,
                  std::string_view needle)
{
    for(const std::pair<meios::level, std::string> &entry : records)
        if(entry.second.find(needle) != std::string::npos)
            return true;
    return false;
}

const char *fanout_doc()
{
    return "<robot xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
           "<xacro:macro name=\"trio\"><link/><link/><link/></xacro:macro>"
           "<xacro:trio/><xacro:trio/><xacro:trio/></robot>";
}

}

TEST_CASE("a shallow-but-wide macro fan-out trips the output-node budget", "[xacro][budget]")
{
    std::vector<std::pair<meios::level, std::string>> records;
    meios::log_sink_f log{ captured_log{ records } };
    meios::source_stack sources;
    meios::eval_scope scope;

    meios::expansion out = meios::expand(fanout_doc(), scope, sources, "robot.xacro",
                                         meios::expansion_limits{ 1'000'000, 4 }, log);

    REQUIRE_FALSE(out.ok);
    REQUIRE(any_contains(records, "budget exceeded"));
    REQUIRE(any_contains(records, "output-node limit"));
}

TEST_CASE("a low work ceiling halts expansion with a resource diagnostic", "[xacro][budget]")
{
    std::vector<std::pair<meios::level, std::string>> records;
    meios::log_sink_f log{ captured_log{ records } };
    meios::source_stack sources;
    meios::eval_scope scope;

    meios::expansion out = meios::expand(fanout_doc(), scope, sources, "robot.xacro",
                                         meios::expansion_limits{ 3, 1'000'000 }, log);

    REQUIRE_FALSE(out.ok);
    REQUIRE(any_contains(records, "budget exceeded"));
    REQUIRE(any_contains(records, "work limit"));
}

TEST_CASE("a generous budget lets an ordinary document expand", "[xacro][budget]")
{
    std::vector<std::pair<meios::level, std::string>> records;
    meios::log_sink_f log{ captured_log{ records } };
    meios::source_stack sources;
    meios::eval_scope scope;

    meios::expansion out = meios::expand(fanout_doc(), scope, sources, "robot.xacro",
                                         meios::expansion_limits{}, log);

    REQUIRE(out.ok);
    REQUIRE_FALSE(any_contains(records, "budget exceeded"));
}

TEST_CASE("a mutual xacro:include chain is caught by the cycle guard", "[xacro][budget][cycle]")
{
    std::vector<std::pair<meios::level, std::string>> records;
    meios::log_sink_f log{ captured_log{ records } };

    meios::memory_source parts;
    parts.add("pkg", "a.xacro",
              "<robot xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
              "<xacro:include filename=\"$(find pkg)/b.xacro\"/></robot>");
    parts.add("pkg", "b.xacro",
              "<robot xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
              "<xacro:include filename=\"$(find pkg)/a.xacro\"/></robot>");
    meios::source_stack sources{ std::move(parts) };
    meios::eval_scope scope;

    const char *top = "<robot xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
                      "<xacro:include filename=\"$(find pkg)/a.xacro\"/></robot>";
    meios::expansion out =
        meios::expand(top, scope, sources, "top.xacro", meios::expansion_limits{}, log);

    REQUIRE_FALSE(out.ok);
    REQUIRE(any_contains(records, "cycle detected"));
    REQUIRE_FALSE(any_contains(records, "budget exceeded"));
}

TEST_CASE("an xacro:include escaping the source root is rejected loudly", "[xacro][budget][root]")
{
    std::vector<std::pair<meios::level, std::string>> records;
    meios::log_sink_f log{ captured_log{ records } };
    meios::memory_source parts;
    parts.add("pkg", "robot.xacro", "<robot/>");
    meios::source_stack sources{ std::move(parts) };
    meios::eval_scope scope;

    const char *top = "<robot xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
                      "<xacro:include filename=\"$(find pkg)/../secret.xacro\"/></robot>";
    meios::expansion out =
        meios::expand(top, scope, sources, "top.xacro", meios::expansion_limits{}, log);

    REQUIRE_FALSE(out.ok);
    REQUIRE(any_contains(records, "escapes the source root"));
}
