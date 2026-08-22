// The resource spec grammar is a second grammar and has to refuse what the asset grammar refuses.
// Every refusal here is asserted by diagnostic code, and the source stack counts what it was asked,
// so a refusal decided ahead of the lookup is observable rather than merely plausible.
#include "yaml_acquisition_fixture.h"
#include "meios/urdf/yaml_resource.h"
#include "meios/urdf/yaml_package_resource.h"

#include <meios/io/source_stack.h>
#include <meios/io/package_source.h>
#include <meios/io/source_lookup.h>
#include <meios/io/resolved_asset.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/source_location.h>
#include <meios/diagnostic/diagnostic_code.h>
#include <meios/diagnostic/operation_failure.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <utility>
#include <optional>
#include <algorithm>
#include <filesystem>
#include <string_view>

namespace
{

struct captured
{
    meios::level           lvl;
    meios::diagnostic_code code;
    meios::source_location loc;
};

struct recorder
{
    std::vector<captured> &sink;

    void operator()(meios::level lvl, const std::string &)
    {
        sink.push_back({ lvl, meios::diagnostic_code::unspecified, {} });
    }

    void operator()(meios::level lvl, const meios::source_location &loc, const std::string &)
    {
        sink.push_back({ lvl, meios::diagnostic_code::unspecified, loc });
    }

    void operator()(meios::level lvl, meios::diagnostic_code code, const meios::source_location &loc,
                    const std::string &)
    {
        sink.push_back({ lvl, code, loc });
    }

    void operator()(meios::level lvl, meios::diagnostic_code code, const meios::source_location &loc,
                    const meios::operation_failure &, const std::string &)
    {
        sink.push_back({ lvl, code, loc });
    }
};

// The counter is a reference because the stack erases the source by value; a counted member would
// be incremented on a copy no case can reach.
struct counting_source
{
    int &calls;
    std::optional<std::filesystem::path> answer;

    meios::capability_descriptor capabilities() const
    {
        return { meios::source_kind::directory, false };
    }

    std::optional<meios::resolved_asset> held() const
    {
        if(!answer)
            return std::nullopt;
        return meios::resolved_asset{ *answer };
    }

    std::optional<meios::resolved_asset> locate(std::string_view, std::string_view)
    {
        return held();
    }

    meios::source_lookup_result try_locate(std::string_view, std::string_view)
    {
        ++calls;
        return held();
    }
};

// One object owns the stack and the roots the loader captures by reference, so a case's loader
// stays valid for exactly as long as the case does.
struct probe
{
    probe(meios::log_sink &log, counting_source source)
        : sources(std::move(source)), roots(),
          loader(meios::detail::make_yaml_text_loader(sources, roots, log))
    {
    }

    meios::source_stack sources;
    std::vector<std::filesystem::path> roots;
    meios::text_resource_loader loader;
};

std::size_t errors_coded(const std::vector<captured> &raised, meios::diagnostic_code code)
{
    return static_cast<std::size_t>(
        std::count_if(raised.begin(), raised.end(), [code](const captured &d) {
            return d.code == code && d.lvl == meios::level::error;
        }));
}

void require_refused_before_lookup(std::string_view spec)
{
    INFO(spec);
    int calls = 0;
    std::vector<captured> raised;
    meios::log_sink_f log{ recorder{ raised } };
    probe under(log, counting_source{ calls, std::nullopt });
    CHECK_FALSE(under.loader(spec, {}).has_value());
    CHECK(errors_coded(raised, meios::diagnostic_code::malformed_asset_uri) == 1);
    CHECK(calls == 0);
}

}

static_assert(meios::package_source<counting_source>);
static_assert(meios::provides_typed_lookup<counting_source>);

TEST_CASE("a malformed package resource spec is refused before the stack is asked", "[yaml][grammar]")
{
    for(std::string_view spec : { "package://pkg/", "package:///rel", "package://pkg//x" })
        require_refused_before_lookup(spec);
}

TEST_CASE("a malformed find-form resource spec is refused before the stack is asked", "[yaml][grammar]")
{
    for(std::string_view spec : { "$(find pkg)/", "$(find )/x" })
        require_refused_before_lookup(spec);
}

TEST_CASE("a well-formed spec no source holds reports absence, not malformedness", "[yaml][grammar]")
{
    int calls = 0;
    std::vector<captured> raised;
    meios::log_sink_f log{ recorder{ raised } };
    probe under(log, counting_source{ calls, std::nullopt });
    CHECK_FALSE(under.loader("package://pkg/cfg.yaml", {}).has_value());
    CHECK(errors_coded(raised, meios::diagnostic_code::unresolved_asset) == 1);
    CHECK(errors_coded(raised, meios::diagnostic_code::malformed_asset_uri) == 0);
    CHECK(calls == 1);
}

TEST_CASE("a well-formed spec a source holds delivers its text after one lookup", "[yaml][grammar]")
{
    const acquisition_test::tree_guard tree;
    int calls = 0;
    std::vector<captured> raised;
    meios::log_sink_f log{ recorder{ raised } };
    probe under(log, counting_source{ calls, tree.path() / "pkg" / "cfg.yaml" });
    CHECK(under.loader("package://pkg/cfg.yaml", {}) == std::optional<std::string>{ "value: 7" });
    CHECK(calls == 1);
    CHECK(raised.empty());
}

TEST_CASE("the package and the find spelling split to the same pair", "[yaml][grammar]")
{
    using meios::detail::yaml_package::package_ref;
    const std::optional<package_ref> named =
        meios::detail::yaml_package::package_split("package://pkg/a");
    const std::optional<package_ref> found =
        meios::detail::yaml_package::find_split("$(find pkg)/a");
    REQUIRE(named.has_value());
    REQUIRE(found.has_value());
    CHECK(named->package == found->package);
    CHECK(named->relative == found->relative);
    CHECK(named->package == "pkg");
    CHECK(named->relative == "a");
}
