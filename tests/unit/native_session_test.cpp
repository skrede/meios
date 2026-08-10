#include "meios/xacro/eval_session.h"
#include "meios/xacro/substitution_detail.h"

#include <meios/xacro.h>
#include <meios/urdf/load.h>

#include <meios/io/source_stack.h>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <cstddef>
#include <fstream>
#include <optional>
#include <filesystem>
#include <functional>
#include <string_view>

#ifndef MEIOS_URDF_FIXTURE_DIR
    #error "MEIOS_URDF_FIXTURE_DIR must name the urdf fixture root"
#endif

namespace
{

// A backend with no failure-state accessor at all still satisfies the seam, which is the
// property that removes the cross-load read. It answers only from the scope it is handed,
// so a shared instance driving two loads at once must produce each load's own result.
struct stateless_backend
{
    meios::text_outcome eval_to_text(std::string_view expr, const meios::eval_scope &scope,
                                     meios::log_sink &)
    {
        const std::optional<meios::value> bound = scope.lookup(expr);
        if(!bound)
            return meios::text_outcome{ std::nullopt, meios::eval_failure_kind::error };
        return meios::text_outcome{ meios::render_scalar(*bound), meios::eval_failure_kind::none };
    }
};

static_assert(meios::text_evaluator<stateless_backend>);

struct tally
{
    std::vector<std::string> messages;

    void operator()(meios::level lvl, const std::string &message)
    {
        if(lvl == meios::level::error)
            messages.push_back(message);
    }

    void operator()(meios::level lvl, const meios::source_location &, const std::string &message)
    {
        (*this)(lvl, message);
    }

    void operator()(meios::level lvl, meios::diagnostic_code, const meios::source_location &,
                    const std::string &message)
    {
        (*this)(lvl, message);
    }
};

bool substitution_survives(meios::eval_policy policy, std::size_t token_ceiling)
{
    const meios::evaluator_limits ceilings{ 0, 0, 0, token_ceiling, 0 };
    meios::detail::eval_session session(ceilings);
    meios::eval_scope scope;
    meios::source_stack sources;
    meios::log_sink silent;
    return meios::detail::substitute_refined("value ${1+1}", scope, sources, "inline.xacro", policy,
                                             {}, session, silent, {})
        .has_value();
}

// The link name is driven through a numeric property, so the substitution layer cannot
// answer it from the scope directly and the shared backend really does run on both threads.
std::filesystem::path write_document(const std::filesystem::path &dir, std::string_view name,
                                     int index)
{
    std::filesystem::create_directories(dir);
    const std::filesystem::path path = dir / std::string(name);
    std::ofstream out(path, std::ios::binary);
    out << "<robot name=\"r\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
        << "<xacro:property name=\"index\" value=\"" << index << "\"/>"
        << "<link name=\"link${index}\"/></robot>";
    return path;
}

std::filesystem::path scratch_root()
{
    return std::filesystem::temp_directory_path() / "meios-native-session";
}

}

TEST_CASE("no evaluator ceiling can be spelled as disabled", "[native][session]")
{
    const meios::evaluator_limits zeroed{ 0, 0, 0, 0, 0, 0 };

    REQUIRE(zeroed.yaml_nodes == meios::default_yaml_node_ceiling);
    REQUIRE(zeroed.yaml_depth == meios::default_yaml_depth_ceiling);
    REQUIRE(zeroed.bytes == meios::default_byte_ceiling);
    REQUIRE(zeroed.tokens == meios::default_token_ceiling);
    REQUIRE(zeroed.steps == meios::default_step_ceiling);
    REQUIRE(zeroed.expression_depth == meios::default_expression_depth_ceiling);

    const meios::evaluator_limits raised{ 7, 8, 9, 10, 11, 12 };
    REQUIRE(raised.yaml_nodes == 7);
    REQUIRE(raised.steps == 11);
    REQUIRE(raised.expression_depth == 12);
}

// The measured sizes of the pinned acceptance target: about 4 500 auxiliary nodes per load
// across four documents, a maximum auxiliary nesting of six, a largest auxiliary document of
// 2 871 bytes, a longest expression of 58 characters, and a deepest expression of 26
// recursive entries -- one bracket level, which is all the target ever nests.
TEST_CASE("the defaults clear the measured acceptance target with headroom", "[native][session]")
{
    const meios::evaluator_limits defaults;

    REQUIRE(defaults.yaml_nodes > 10 * 4500);
    REQUIRE(defaults.yaml_depth > 10 * 6);
    REQUIRE(defaults.bytes > 10 * 2871);
    REQUIRE(defaults.tokens > 10 * 58);
    REQUIRE(defaults.steps > 10 * 58);
    REQUIRE(defaults.expression_depth > 9 * 26);
}

// Measured by driving a balanced parenthesis nest until the process died, on Linux x86-64
// with an optimized build: 38 088 recursive entries against the default 8192 KiB stack, and
// 2 396 against a 512 KiB thread. The smaller thread is the binding case, and re-measuring
// on another machine is what these two numbers are here to invite.
TEST_CASE("the nesting ceiling stays below the depth that exhausts the stack", "[native][session]")
{
    constexpr std::size_t exhausted_on_a_main_stack = 38088;
    constexpr std::size_t exhausted_on_a_small_thread = 2396;

    const meios::evaluator_limits defaults;

    REQUIRE(defaults.expression_depth * 9 < exhausted_on_a_small_thread);
    REQUIRE(defaults.expression_depth * 100 < exhausted_on_a_main_stack);
}

TEST_CASE("a session starts every counter at zero and charges before the work", "[native][session]")
{
    const meios::evaluator_limits ceilings;
    meios::detail::eval_session session(ceilings);
    meios::log_sink silent;

    REQUIRE(session.counters.tokens == 0);
    REQUIRE(session.counters.bytes == 0);
    REQUIRE(session.counters.steps == 0);
    REQUIRE(session.counters.yaml_nodes == 0);
    REQUIRE(session.counters.yaml_depth == 0);
    REQUIRE(session.counters.expression_depth == 0);
    REQUIRE(session.failure == meios::eval_failure_kind::none);

    REQUIRE(session.charge_tokens(4, silent, {}));
    REQUIRE(session.counters.tokens == 4);
    REQUIRE(session.admits_yaml_depth(3, silent, {}));
    REQUIRE(session.counters.yaml_depth == 3);
    REQUIRE(session.admits_expression_depth(5, silent, {}));
    REQUIRE(session.counters.expression_depth == 5);
    REQUIRE(session.failure == meios::eval_failure_kind::none);
}

TEST_CASE("a crossed ceiling latches an exhausted failure that stays latched", "[native][session]")
{
    const meios::evaluator_limits ceilings{ 4, 2, 16, 8, 32, 3 };
    meios::detail::eval_session session(ceilings);
    tally counts;
    meios::log_sink_f sink{ std::ref(counts) };

    REQUIRE_FALSE(session.charge_tokens(9, sink, {}));
    REQUIRE(session.failure == meios::eval_failure_kind::exhausted);
    REQUIRE(session.terminal.has_value());
    REQUIRE(session.terminal->code == meios::diagnostic_code::expansion_budget_exceeded);
    REQUIRE(counts.messages.size() == 1);

    REQUIRE_FALSE(session.charge_bytes(1, sink, {}));
    REQUIRE_FALSE(session.admits_yaml_depth(1, sink, {}));
    REQUIRE_FALSE(session.admits_expression_depth(1, sink, {}));
    REQUIRE(counts.messages.size() == 1);
}

TEST_CASE("exhaustion is terminal under every evaluation policy", "[native][session]")
{
    REQUIRE(substitution_survives(meios::eval_policy::fail, 64));
    REQUIRE(substitution_survives(meios::eval_policy::warn, 64));
    REQUIRE(substitution_survives(meios::eval_policy::skip, 64));

    REQUIRE_FALSE(substitution_survives(meios::eval_policy::fail, 1));
    REQUIRE_FALSE(substitution_survives(meios::eval_policy::warn, 1));
    REQUIRE_FALSE(substitution_survives(meios::eval_policy::skip, 1));
}

TEST_CASE("a scope with no installed parser names the missing capability", "[native][session]")
{
    const meios::evaluator_limits ceilings;
    meios::evaluator_counters counters;
    meios::eval_scope scope;
    tally counts;
    meios::log_sink_f sink{ std::ref(counts) };

    const meios::yaml_outcome parsed = scope.parse_yaml("a: 1", ceilings, counters, sink, {});

    REQUIRE_FALSE(parsed.parsed.has_value());
    REQUIRE(parsed.failure == meios::yaml_failure::unavailable);
    REQUIRE(counts.messages.size() == 1);
    REQUIRE(counts.messages.front().find("resolves no auxiliary document format")
            != std::string::npos);
}

TEST_CASE("two loads over one shared configuration stay independent", "[native][session]")
{
    const std::filesystem::path dir = scratch_root();
    const std::filesystem::path first = write_document(dir, "alpha.xacro", 3);
    const std::filesystem::path second = write_document(dir, "beta.xacro", 8);

    meios::load_options shared;
    shared.backend = std::make_shared<meios::evaluator_handle>(stateless_backend{});

    std::optional<std::string> alpha_link;
    std::optional<std::string> beta_link;
    std::thread one([&] {
        const auto loaded = meios::load(first, shared);
        if(loaded && loaded->robot.links.size() == 1)
            alpha_link = loaded->robot.links.front().name;
    });
    std::thread two([&] {
        const auto loaded = meios::load(second, shared);
        if(loaded && loaded->robot.links.size() == 1)
            beta_link = loaded->robot.links.front().name;
    });
    one.join();
    two.join();

    REQUIRE(alpha_link == std::optional<std::string>("link3"));
    REQUIRE(beta_link == std::optional<std::string>("link8"));

    std::filesystem::remove_all(dir);
}
