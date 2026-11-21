#include <meios/io/scratch_dir.h>
#include <meios/io/text_reader.h>

#include <catch2/catch_test_macros.hpp>

#if defined(_WIN32)

    #include <windows.h>

    #include <string>
    #include <cstdlib>
    #include <fstream>
    #include <filesystem>
    #include <string_view>
    #include <system_error>

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

bool ran(const std::string &command)
{
    const std::string quiet = command + " >NUL 2>&1";
    return std::system(quiet.c_str()) == 0;
}

std::string quoted(const std::filesystem::path &path)
{
    return '"' + path.string() + '"';
}

bool reparse_point(const std::filesystem::path &path)
{
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}

std::string repeated_text()
{
    std::string text;
    while(text.size() < 64U * 1024U)
        text += "a line of text the file system compresses well\n";
    return text;
}

}

TEST_CASE("a file compressed behind a reparse point reads its content under a root", "[io][reparse]")
{
    const meios::scratch_dir tree    = fresh_tree();
    const std::filesystem::path file = tree.path() / "compressed.txt";
    const std::string text           = repeated_text();
    write_file(file, text);
    if(!ran("compact /c /exe:xpress4k " + quoted(file)) || !reparse_point(file))
        SKIP("this volume keeps no file compressed behind a reparse point");

    const meios::text_read_result read = meios::read_text_file_under(tree.path(), "compressed.txt");
    REQUIRE(read.has_value());
    REQUIRE(*read == text);
}

TEST_CASE("a junction under a root is a link that leaves it", "[io][reparse]")
{
    const meios::scratch_dir outside = fresh_tree();
    const meios::scratch_dir held    = fresh_tree();
    write_file(outside.path() / "target" / "f.txt", "outside");
    if(!ran("cmd /c mklink /J " + quoted(held.path() / "junction") + " " + quoted(outside.path() / "target")))
        SKIP("this host cannot create a junction");

    const meios::text_read_result read = meios::read_text_file_under(held.path(), "junction/f.txt");
    REQUIRE_FALSE(read.has_value());
    REQUIRE(read.error().kind == meios::text_read_failure_kind::uncontained);
}

#endif
