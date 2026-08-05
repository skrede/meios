#include <meios/xacro.h>

#include <meios/io/source_handle.h>
#include <meios/io/source_stack.h>
#include <meios/io/directory_source.h>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <random>
#include <string>
#include <vector>
#include <fstream>
#include <utility>
#include <iterator>
#include <optional>
#include <filesystem>
#include <string_view>

namespace
{

using expansion_result = meios::expected<meios::expansion, meios::expansion_error>;

// Records "<document>|<spec>" for every request, so a case observes which document the
// scope hands the loader without the scope exposing an accessor for it.
struct recording_fetch final : meios::text_resource_loader::fetcher
{
    recording_fetch(std::vector<std::string> &seen, std::optional<std::string> reply)
        : m_seen(seen), m_reply(std::move(reply))
    {
    }

    std::optional<std::string> fetch(std::string_view spec,
                                     const std::filesystem::path &document) override
    {
        m_seen.push_back(document.generic_string() + '|' + std::string(spec));
        return m_reply;
    }

    std::vector<std::string> &m_seen;
    std::optional<std::string> m_reply;
};

meios::text_resource_loader recorder(std::vector<std::string> &seen,
                                     std::optional<std::string> reply = std::string("loaded"))
{
    return meios::text_resource_loader{
        std::make_unique<recording_fetch>(seen, std::move(reply)) };
}

// Routes every expression through the scope's loader, which is what lets an expansion
// case observe the active document moving with the include stack.
struct loading_backend
{
    std::optional<std::string> eval_to_text(std::string_view expr, const meios::eval_scope &scope,
                                            meios::log_sink &)
    {
        return scope.load_text(expr);
    }

    meios::eval_failure_kind last_failure_kind() const { return meios::eval_failure_kind::none; }
};

std::filesystem::path fresh_dir()
{
    std::random_device device;
    for(;;)
    {
        std::filesystem::path candidate = std::filesystem::temp_directory_path()
                                        / ("meios-scope-" + std::to_string(device()));
        if(!std::filesystem::exists(candidate))
            return candidate;
    }
}

std::filesystem::path seed_include_tree()
{
    std::filesystem::path root = fresh_dir();
    std::filesystem::create_directories(root / "inner");
    std::ofstream(root / "top.xacro")
        << R"(<robot xmlns:xacro="http://ros.org/wiki/xacro"><a v="${outer}"/>)"
        << R"(<xacro:include filename="inner/leaf.xacro"/><b v="${after}"/></robot>)";
    std::ofstream(root / "inner" / "leaf.xacro")
        << R"(<robot xmlns:xacro="http://ros.org/wiki/xacro"><c v="${inner}"/></robot>)";
    return root;
}
}

TEST_CASE("a scope carrying no text loader refuses instead of reading", "[xacro][scope]")
{
    const meios::eval_scope scope;
    REQUIRE_FALSE(scope.load_text("cfg.yaml").has_value());
}

TEST_CASE("an installed text loader answers through a const scope", "[xacro][scope]")
{
    std::vector<std::string> seen;
    meios::eval_scope scope;
    scope.install_text_loader(recorder(seen));

    const meios::eval_scope &sealed = scope;
    REQUIRE(sealed.load_text("anything") == std::string("loaded"));
    REQUIRE(seen.size() == 1);
}

TEST_CASE("a refusing loader reads no differently from an absent one at the carrier", "[xacro][scope]")
{
    std::vector<std::string> seen;
    meios::eval_scope scope;
    scope.install_text_loader(recorder(seen, std::nullopt));

    REQUIRE_FALSE(scope.load_text("cfg.yaml").has_value());
    REQUIRE(seen.size() == 1);
}

TEST_CASE("the loader is handed the document the scope is active on", "[xacro][scope]")
{
    std::vector<std::string> seen;
    meios::eval_scope scope;
    scope.install_text_loader(recorder(seen));

    scope.load_text("cfg.yaml");
    REQUIRE(seen.back() == "|cfg.yaml");

    scope.set_active_document("/a/b/top.xacro");
    scope.load_text("cfg.yaml");
    REQUIRE(seen.back() == "/a/b/top.xacro|cfg.yaml");

    scope.set_active_document("/a/c/other.xacro");
    scope.load_text("cfg.yaml");
    REQUIRE(seen.back() == "/a/c/other.xacro|cfg.yaml");
}

TEST_CASE("the active document follows the include stack through an expansion", "[xacro][scope]")
{
    const std::filesystem::path root = seed_include_tree();
    const std::filesystem::path top = std::filesystem::weakly_canonical(root / "top.xacro");
    const std::filesystem::path leaf =
        std::filesystem::weakly_canonical(root / "inner" / "leaf.xacro");

    std::vector<std::string> seen;
    meios::log_sink sink;
    meios::source_stack sources;
    sources.push_back(meios::source_handle(meios::directory_source(root, sink)));

    std::string source;
    {
        std::ifstream in(top, std::ios::binary);
        source.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }

    meios::eval_scope scope;
    scope.install_text_loader(recorder(seen));
    const std::shared_ptr<meios::evaluator_handle> backend =
        std::make_shared<meios::evaluator_handle>(loading_backend{});
    const expansion_result out = meios::expand(source, scope, sources, top,
                                               meios::expansion_limits{},
                                               meios::eval_policy::fail, backend, sink);

    std::filesystem::remove_all(root);

    REQUIRE(out.has_value());
    REQUIRE(seen.size() == 3);
    REQUIRE(seen[0] == top.generic_string() + "|outer");
    REQUIRE(seen[1] == leaf.generic_string() + "|inner");
    REQUIRE(seen[2] == top.generic_string() + "|after");
}