#include <meios/scan/stl_scanner.h>

#include <meios/io.h>
#include <meios/bundle.h>

#include <string>
#include <vector>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <string_view>
#include <system_error>

namespace
{

constexpr const char *base_stl =
    "solid base\n"
    "  facet normal 0 0 1\n"
    "    outer loop\n"
    "      vertex 0 0 0\n"
    "      vertex 1 0 0\n"
    "      vertex 0 1 0\n"
    "    endloop\n"
    "  endfacet\n"
    "endsolid base\n";

void write_fixture(const std::filesystem::path &path)
{
    std::ofstream out(path);
    out << base_stl;
}

void report(std::string_view extension, bool registered, std::size_t found)
{
    std::cout << "  ." << extension << ": "
              << (registered ? "a scanner is registered" : "no scanner is registered")
              << ", the dispatched scan returned " << found << " reference(s) -- "
              << (registered ? "a scanner answered, and this format carries no external reference"
                             : "no scanner answered at all")
              << '\n';
}

bool the_two_empties_are_told_apart(const std::filesystem::path &path, meios::log_sink &log)
{
    meios::scanner_registry registry;
    meios::register_stl_scanner(registry);
    const meios::resolved_asset asset(path);

    const bool stl_registered = registry.contains("stl");
    const bool ply_registered = registry.contains("ply");

    std::cout << "the same empty result from two different causes:\n";
    report("stl", stl_registered, registry.scan("stl", asset, log).size());
    report("ply", ply_registered, registry.scan("ply", asset, log).size());

    if(!stl_registered)
        std::cout << "the stl scanner was registered and the registry does not report carrying it\n";
    else if(ply_registered)
        std::cout << "the registry reports carrying a scanner for an extension never registered\n";
    else
        return true;
    return false;
}

}

int main()
{
    const std::filesystem::path stl_path =
        std::filesystem::temp_directory_path() / "meios_example_base.stl";
    write_fixture(stl_path);

    meios::log_sink_s log(std::cout);
    const bool told_apart = the_two_empties_are_told_apart(stl_path, log);

    std::error_code ec;
    std::filesystem::remove(stl_path, ec);
    return told_apart ? 0 : 1;
}
