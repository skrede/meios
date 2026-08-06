#include <meios/scan/gltf_scanner.h>

#include <meios/io.h>

#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <string_view>
#include <system_error>

namespace
{

constexpr const char *arm_gltf =
    "{\n"
    "  \"asset\": { \"version\": \"2.0\" },\n"
    "  \"images\": [\n"
    "    { \"uri\": \"textures/steel%20plate.png\" },\n"
    "    { \"uri\": \"data:image/png;base64,iVBORw0KGgoAAAANSUhEUg==\" }\n"
    "  ],\n"
    "  \"buffers\": [ { \"uri\": \"arm.bin\" } ]\n"
    "}\n";

void write_fixture(const std::filesystem::path &path)
{
    std::ofstream out(path);
    out << arm_gltf;
}

bool holds_inline_data(const std::vector<std::string> &references)
{
    return std::any_of(references.begin(), references.end(),
                       [](std::string_view reference) { return reference.starts_with("data:"); });
}

bool scanned_the_external_ones(const std::filesystem::path &path, meios::log_sink &log)
{
    meios::gltf_scanner scanner;
    const meios::resolved_asset asset(path);
    const std::vector<std::string> references = scanner.scan(asset, log);

    std::cout << "the gltf scanner found " << references.size() << " external referenced asset(s):\n";
    for(const std::string &reference : references)
        std::cout << "  " << reference << '\n';

    if(references.empty())
        std::cout << "the document carries an external image and an external buffer, "
                     "and the scan reported neither\n";
    else if(holds_inline_data(references))
        std::cout << "an inline data reference was reported, but a document carries its own bytes "
                     "and nothing needs to travel beside it\n";
    else
        return true;
    return false;
}

}

int main()
{
    const std::filesystem::path gltf_path =
        std::filesystem::temp_directory_path() / "meios_example_arm.gltf";
    write_fixture(gltf_path);

    meios::log_sink_s log(std::cout);
    const bool found = scanned_the_external_ones(gltf_path, log);

    std::error_code ec;
    std::filesystem::remove(gltf_path, ec);
    return found ? 0 : 1;
}
