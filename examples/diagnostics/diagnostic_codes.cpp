#include <meios/urdf.h>
#include <meios/model.h>

#include <meios/diagnostic/capturing_log_sink.h>

#include <vector>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <system_error>

namespace
{

// A vendor extension block and a material the document never declares are both ordinary content of
// shipped robot descriptions, and both are reported under the default policies; nothing here is
// loosened to provoke them.
std::filesystem::path write_description()
{
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "meios_codes_example.urdf";
    std::ofstream out(path);
    out << "<?xml version=\"1.0\"?>\n"
           "<robot name=\"codes_arm\">\n"
           "  <link name=\"base_link\">\n"
           "    <visual><geometry><box size=\"1 1 1\"/></geometry>\n"
           "      <material name=\"never_declared\"/></visual>\n"
           "  </link>\n"
           "  <gazebo reference=\"base_link\"><mu1>0.2</mu1></gazebo>\n"
           "</robot>\n";
    return path;
}

std::vector<meios::captured_diagnostic> capture_records()
{
    const std::filesystem::path path = write_description();

    meios::log_sink_s stream(std::cerr);
    meios::capturing_log_sink capture(stream);
    const meios::capture_window window(capture);

    const meios::load_options options;
    meios::load(path, options, window.log());

    std::error_code ec;
    std::filesystem::remove(path, ec);

    return window.records();
}

bool every_code_named(const std::vector<meios::captured_diagnostic> &records)
{
    for(const meios::captured_diagnostic &record : records)
        if(meios::to_string(record.code).empty())
            return false;
    return true;
}

}

int main()
{
    const std::vector<meios::captured_diagnostic> records = capture_records();
    if(records.empty())
    {
        std::cout << "nothing was captured, so there is no record here to read back\n";
        return 1;
    }

    for(const meios::captured_diagnostic &record : records)
        std::cout << "code " << meios::to_string(record.code) << " | at "
                  << meios::to_string(record.loc) << " | says " << record.message << '\n';

    if(!every_code_named(records))
    {
        std::cout << "a captured code carries no name, so the converter and the enumeration have "
                     "drifted apart\n";
        return 1;
    }

    return 0;
}
