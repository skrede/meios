#include "acquisition_script.h"

#include <meios/io/scratch_dir.h>
#include <meios/io/text_reader.h>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <string>
#include <vector>
#include <fstream>
#include <optional>
#include <filesystem>
#include <string_view>
#include <system_error>

#if !defined(_WIN32)
    #include <sys/stat.h>
#endif

namespace
{

meios::text_read_result run(acquisition_test::reader_script &script)
{
    acquisition_test::scripted_operations operations{script};
    return meios::detail::read_text_file("scripted", operations);
}

void require_failure(const meios::text_read_result &result, meios::text_read_failure_kind kind, meios::operation_kind operation, int value)
{
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().kind == kind);
    REQUIRE(result.error().cause.operation == operation);
    REQUIRE(result.error().cause.native.value() == value);
}

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

TEST_CASE("scripted operation refusals stop at their failing boundary", "[text_reader]")
{
    acquisition_test::reader_script status;
    status.status_error = {11, acquisition_test::category};
    require_failure(run(status), meios::text_read_failure_kind::status, meios::operation_kind::status, 11);
    REQUIRE(status.calls == std::vector<std::string>{"status"});

    acquisition_test::reader_script open;
    open.open_error = {13, acquisition_test::category};
    require_failure(run(open), meios::text_read_failure_kind::open, meios::operation_kind::open, 13);
    REQUIRE(open.calls == std::vector<std::string>{"status", "open"});

    acquisition_test::reader_script read;
    read.read_error = {17, acquisition_test::category};
    require_failure(run(read), meios::text_read_failure_kind::read, meios::operation_kind::read, 17);
    REQUIRE(read.calls == std::vector<std::string>{"status", "open", "read", "error", "close"});
    REQUIRE(read.closes == 1);
}

TEST_CASE("zero-error and over-transfer backend failures are deterministic", "[text_reader]")
{
    acquisition_test::reader_script zero_read;
    zero_read.fail_read                = true;
    const meios::text_read_result read = run(zero_read);
    require_failure(read, meios::text_read_failure_kind::read, meios::operation_kind::read, int(std::errc::io_error));
    REQUIRE(&read.error().cause.native.category() == &std::generic_category());

    acquisition_test::reader_script zero_close;
    zero_close.fail_close               = true;
    const meios::text_read_result close = run(zero_close);
    require_failure(close, meios::text_read_failure_kind::close, meios::operation_kind::close, int(std::errc::io_error));

    acquisition_test::reader_script over;
    over.over_transfer                        = 1;
    const meios::text_read_result transferred = run(over);
    require_failure(transferred, meios::text_read_failure_kind::read, meios::operation_kind::read, int(std::errc::io_error));
    REQUIRE(over.closes == 1);
}

TEST_CASE("empty binary and multi-chunk content remain exact", "[text_reader]")
{
    acquisition_test::reader_script empty;
    empty.content.clear();
    const meios::text_read_result empty_result = run(empty);
    REQUIRE(empty_result.has_value());
    REQUIRE(empty_result->empty());

    acquisition_test::reader_script binary;
    binary.chunks                        = {"one", std::string{"t\0wo", 4}, "three"};
    const meios::text_read_result result = run(binary);
    REQUIRE(result.has_value());
    REQUIRE(*result == std::string{"onet\0wothree", 12});
    REQUIRE(binary.request == 65536);
    REQUIRE(binary.closes == 1);
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
