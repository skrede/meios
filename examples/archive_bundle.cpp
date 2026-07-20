#include <meios/archive/zip_writer.h>

#include <iostream>
#include <filesystem>
#include <string_view>
#include <system_error>

int main()
{
    const std::filesystem::path archive_path =
        std::filesystem::temp_directory_path() / "meios_example_bundle.zip";

    meios::log_sink log;
    meios::zip_writer writer(archive_path, log);

    const std::string_view robot_name = "arm.urdf";
    const std::string_view robot_text =
        "<?xml version=\"1.0\"?>\n<robot name=\"arm\"><link name=\"base_link\"/></robot>\n";
    const meios::asset_manifest manifest;

    const meios::emit_result result = writer.write(robot_name, robot_text, manifest, false);

    std::cout << "archive-zip writer finished with status " << meios::to_string(result.status)
              << " (" << result.assets_seen << " asset(s) seen)\n";

    std::error_code ec;
    std::filesystem::remove(archive_path, ec);
    return 0;
}
