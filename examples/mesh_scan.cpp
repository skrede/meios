#include <meios/scan/obj_scanner.h>

#include <meios/io.h>

#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <system_error>

int main()
{
    const std::filesystem::path obj_path =
        std::filesystem::temp_directory_path() / "meios_example_mesh.obj";
    {
        std::ofstream out(obj_path);
        out << "mtllib arm.mtl\n";
        out << "usemtl steel\n";
        out << "v 0 0 0\n";
    }

    meios::log_sink log;
    meios::obj_scanner scanner;
    meios::resolved_asset asset(obj_path);
    const std::vector<std::string> references = scanner.scan(asset, log);

    std::cout << "obj scanner found " << references.size() << " referenced asset(s):\n";
    for(const std::string &reference : references)
        std::cout << "  " << reference << '\n';

    std::error_code ec;
    std::filesystem::remove(obj_path, ec);
    return 0;
}
