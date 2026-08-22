#include <meios/scan/collada_scanner.h>

#include <meios/io.h>

#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <system_error>

namespace
{

constexpr const char *arm_dae =
    "<?xml version=\"1.0\"?>\n"
    "<COLLADA version=\"1.4.1\">\n"
    "  <library_images>\n"
    "    <image id=\"steel\"><init_from>textures/steel%20plate.png</init_from></image>\n"
    "    <image id=\"decal\"><init_from>file://textures/decal.png</init_from></image>\n"
    "  </library_images>\n"
    "</COLLADA>\n";

void write_fixture(const std::filesystem::path &path)
{
    std::ofstream out(path);
    out << arm_dae;
}

bool scanned_the_images(const std::filesystem::path &path, meios::log_sink &log)
{
    meios::collada_scanner scanner;
    const meios::resolved_asset asset(path);
    const std::vector<std::string> references = scanner.scan(asset, log);

    std::cout << "the collada scanner found " << references.size() << " referenced asset(s):\n";
    for(const std::string &reference : references)
        std::cout << "  " << reference << '\n';

    if(references.empty())
    {
        std::cout << "the document names two images and the scan reported none\n";
        return false;
    }
    return true;
}

}

int main()
{
    const std::filesystem::path dae_path =
        std::filesystem::temp_directory_path() / "meios_example_arm.dae";
    write_fixture(dae_path);

    meios::log_sink_s log(std::cout);
    const bool found = scanned_the_images(dae_path, log);

    std::error_code ec;
    std::filesystem::remove(dae_path, ec);
    return found ? 0 : 1;
}
