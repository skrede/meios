#include "meios/io/text_reader_operations.h"

#include <meios/io/scratch_dir.h>
#include <meios/io/text_reader.h>

#include <meios/expected.h>

#include <meios/diagnostic/operation_failure.h>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <string>
#include <cstddef>
#include <fstream>
#include <filesystem>
#include <string_view>
#include <system_error>

#if !defined(_WIN32)
    #include <sys/stat.h>
#endif

namespace
{

meios::scratch_dir fresh_tree()
{
    std::error_code error;
    const std::filesystem::path parent = std::filesystem::temp_directory_path(error);
    REQUIRE_FALSE(error);
    const meios::expected<std::filesystem::path, meios::operation_failure> root = meios::detail::create_scratch_root(parent);
    REQUIRE(root.has_value());
    return meios::scratch_dir{*root};
}

void write_file(const std::filesystem::path &path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    output.close();
    REQUIRE(output.good());
}

void require_link(const std::filesystem::path &target, const std::filesystem::path &link, bool directory)
{
    std::error_code error;
    if(directory)
        std::filesystem::create_directory_symlink(target, link, error);
    else
        std::filesystem::create_symlink(target, link, error);
    CAPTURE(error.message());
    REQUIRE_FALSE(error);
}

}

TEST_CASE("native ordinary failures retain the CRT error domain", "[text_reader]")
{
    meios::scratch_dir tree              = fresh_tree();
    const meios::text_read_result result = meios::read_text_file(tree.path() / "missing");
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().kind == meios::text_read_failure_kind::open);
    REQUIRE(&result.error().cause.native.category() == &std::generic_category());
    REQUIRE(result.error().cause.native.value() != 0);
}

TEST_CASE("an ordinary read follows a link to a regular file and still refuses other kinds", "[text_reader]")
{
    meios::scratch_dir tree = fresh_tree();
    write_file(tree.path() / "real.txt", "through-link");
    std::filesystem::create_directories(tree.path() / "sub");
    require_link(tree.path() / "real.txt", tree.path() / "file-link", false);
    require_link(tree.path() / "sub", tree.path() / "directory-link", true);

    const meios::text_read_result linked = meios::read_text_file(tree.path() / "file-link");
    REQUIRE(linked.has_value());
    REQUIRE(*linked == "through-link");
    REQUIRE_FALSE(meios::read_text_file(tree.path() / "directory-link").has_value());

#if !defined(_WIN32)
    REQUIRE(::mkfifo((tree.path() / "pipe").c_str(), 0600) == 0);
    require_link(tree.path() / "pipe", tree.path() / "pipe-link", false);
    REQUIRE_FALSE(meios::read_text_file(tree.path() / "pipe-link").has_value());
#endif
}

TEST_CASE("contained reads reject links directories and special files", "[text_reader]")
{
    meios::scratch_dir tree    = fresh_tree();
    meios::scratch_dir outside = fresh_tree();
    write_file(tree.path() / "inside" / "ok.txt", "inside");
    write_file(outside.path() / "outside.txt", "outside");
    require_link(tree.path() / "inside" / "ok.txt", tree.path() / "file-link", false);
    require_link(tree.path() / "inside", tree.path() / "directory-link", true);
    require_link(outside.path() / "outside.txt", tree.path() / "outside-link", false);
    require_link(tree.path(), outside.path() / "root-link", true);

    const meios::text_read_result accepted = meios::detail::read_text_file_under(tree.path(), "inside/ok.txt");
    REQUIRE(accepted.has_value());
    REQUIRE(*accepted == "inside");
    for(const std::filesystem::path relative : {"file-link", "directory-link/ok.txt", "outside-link", "inside", "../outside.txt"})
        REQUIRE_FALSE(meios::detail::read_text_file_under(tree.path(), relative).has_value());
    REQUIRE_FALSE(meios::detail::read_text_file_under(outside.path() / "root-link", "inside/ok.txt").has_value());

#if !defined(_WIN32)
    REQUIRE(::mkfifo((tree.path() / "pipe").c_str(), 0600) == 0);
    REQUIRE_FALSE(meios::detail::read_text_file_under(tree.path(), "pipe").has_value());
    REQUIRE_FALSE(meios::detail::read_text_file_under("/", "dev/null").has_value());
#endif
}

TEST_CASE("a contained directory is refused by kind on every platform", "[text_reader]")
{
    meios::scratch_dir tree = fresh_tree();
    write_file(tree.path() / "inside" / "ok.txt", "inside");

    const meios::text_read_result refused = meios::detail::read_text_file_under(tree.path(), "inside");

    REQUIRE_FALSE(refused.has_value());
    REQUIRE(refused.error().kind == meios::text_read_failure_kind::non_regular);
    REQUIRE(refused.error().cause.operation == meios::operation_kind::status);
}

#if !defined(_WIN32)
TEST_CASE("an opened file remains the read authority after path replacement", "[text_reader]")
{
    meios::scratch_dir tree          = fresh_tree();
    const std::filesystem::path path = tree.path() / "asset.txt";
    write_file(path, "opened");
    meios::detail::text_open_result opened = meios::detail::default_text_reader_operations().open_checked(path);
    REQUIRE(opened.has_value());

    std::filesystem::rename(path, tree.path() / "original.txt");
    write_file(path, "replacement");
    std::array<char, 16> buffer{};
    const std::size_t transferred = (*opened)->read(buffer);

    REQUIRE_FALSE((*opened)->failed());
    REQUIRE(std::string_view{buffer.data(), transferred} == "opened");
    REQUIRE((*opened)->close());
}
#endif
