#include "yaml_resource_fixture.h"
#include "native_yaml_seam_fixture.h"

#include <meios/io/text_reader.h>
#include <meios/io/source_stack.h>

#include <meios/diagnostic/operation_failure.h>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <vector>
#include <cstddef>
#include <fstream>
#include <filesystem>
#include <system_error>

using namespace yaml_seam_test;

namespace
{

constexpr std::size_t short_size = 16;

// The sweep hands open_under the canonical root, so a root is keyed canonically and still matches
// where the temporary directory is spelled through a link.
std::filesystem::path root_key(const std::filesystem::path &root)
{
    std::error_code ec;
    std::filesystem::path canonical = std::filesystem::weakly_canonical(root, ec);
    return ec ? root.lexically_normal() : canonical;
}

// Serves an oversized document under one root, fails the open under another, and serves a short
// document anywhere else, so what comes back shows which root the sweep stopped at.
class rooted_operations final : public meios::detail::text_reader_operations
{
public:
    rooted_operations(const std::filesystem::path &oversized, const std::filesystem::path &failing, std::size_t &delivered)
            : m_delivered(delivered)
            , m_failing(root_key(failing))
            , m_oversized(root_key(oversized))
    {
    }

    meios::detail::text_status_result status(const std::filesystem::path &) const noexcept override
    {
        return std::filesystem::file_status(std::filesystem::file_type::regular);
    }

    meios::detail::text_file_result open(const std::filesystem::path &) const noexcept override
    {
        return std::make_unique<generated_file>(short_size, m_delivered);
    }

    meios::detail::text_open_result open_under(const std::filesystem::path &root, const std::filesystem::path &) const noexcept override
    {
        const std::filesystem::path key = root_key(root);
        if(key == m_oversized)
            return std::make_unique<generated_file>(generated_size, m_delivered);
        if(key == m_failing)
            return meios::unexpected<meios::text_read_failure>({meios::text_read_failure_kind::open, {meios::operation_kind::open, make_error_code(std::errc::io_error)}});
        return std::make_unique<generated_file>(short_size, m_delivered);
    }

private:
    std::size_t &m_delivered;
    std::filesystem::path m_failing;
    std::filesystem::path m_oversized;
};

struct two_roots
{
    std::filesystem::path base;
    std::filesystem::path first;
    std::filesystem::path second;
};

two_roots seed_roots()
{
    const std::filesystem::path base = yaml_resource_test::fresh_dir();
    const two_roots tree{base, base / "first", base / "second"};
    std::filesystem::create_directories(tree.first);
    std::filesystem::create_directories(tree.second);
    std::ofstream(tree.first / "cfg.yaml") << "from: first";
    std::ofstream(tree.second / "cfg.yaml") << "from: second";
    return tree;
}

meios::text_resource_result sweep(const two_roots &tree, const meios::detail::text_reader_operations &operations, recording_sink &heard)
{
    meios::source_stack sources;
    const std::vector<std::filesystem::path> roots{tree.second};
    null_probe probe;
    const meios::text_resource_loader loader = meios::detail::make_yaml_text_loader(sources, roots, heard, operations, probe, read_ceiling);
    return loader("cfg.yaml", tree.first / "robot.xacro");
}

}

TEST_CASE("an oversized document in an earlier root ends the sweep instead of yielding a later root's copy", "[native][yaml]")
{
    const two_roots tree  = seed_roots();
    std::size_t delivered = 0;
    const rooted_operations operations(tree.first, tree.base / "none", delivered);
    recording_sink heard;

    const meios::text_resource_result result = sweep(tree, operations, heard);
    std::filesystem::remove_all(tree.base);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == meios::text_resource_failure::exhausted);
    CHECK(heard.messages.empty());
}

TEST_CASE("an oversized document in a later root outranks a native failure in an earlier one", "[native][yaml]")
{
    const two_roots tree  = seed_roots();
    std::size_t delivered = 0;
    const rooted_operations operations(tree.second, tree.first, delivered);
    recording_sink heard;

    const meios::text_resource_result result = sweep(tree, operations, heard);
    std::filesystem::remove_all(tree.base);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == meios::text_resource_failure::exhausted);
    CHECK(heard.messages.empty());
}

TEST_CASE("a native failure in an earlier root still recovers at a later root holding the document", "[native][yaml]")
{
    const two_roots tree  = seed_roots();
    std::size_t delivered = 0;
    const rooted_operations operations(tree.base / "none", tree.first, delivered);
    recording_sink heard;

    const meios::text_resource_result result = sweep(tree, operations, heard);
    std::filesystem::remove_all(tree.base);

    REQUIRE(result.has_value());
    CHECK(result->size() == short_size);
    CHECK(heard.messages.empty());
}
