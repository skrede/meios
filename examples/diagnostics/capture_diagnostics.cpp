#include "example_paths.h"

#include <meios/urdf.h>
#include <meios/model.h>

#include <meios/diagnostic/capturing_log_sink.h>

#include <fstream>
#include <iostream>
#include <optional>
#include <filesystem>
#include <system_error>

namespace
{

// The package named here is deliberately absent, and no option is set to make it acceptable: the
// default missing-asset policy refuses an asset it cannot find, and refusing loudly is the thing
// this program captures.
std::filesystem::path write_flawed_description()
{
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "meios_capture_example.urdf";
    std::ofstream out(path);
    out << "<?xml version=\"1.0\"?>\n"
           "<robot name=\"capture_arm\">\n"
           "  <link name=\"base_link\">\n"
           "    <visual>\n"
           "      <geometry><mesh filename=\"package://absent_package/missing.dae\"/></geometry>\n"
           "    </visual>\n"
           "  </link>\n"
           "</robot>\n";
    return path;
}

// The returned outcome is dropped on purpose: this program answers from the window, which holds
// what the load reported whether that load went on to succeed or not.
void load_through(const meios::capture_window &window, const std::filesystem::path &path)
{
    const meios::load_options options;
    meios::load(path, options, window.log());
}

void load_flawed(const meios::capture_window &window)
{
    const std::filesystem::path path = write_flawed_description();
    load_through(window, path);

    std::error_code ec;
    std::filesystem::remove(path, ec);
}

void report(const char *subject, const meios::capture_window &window)
{
    std::cout << subject << " left " << window.records().size() << " record(s) in its window, "
              << window.errors() << " of them error(s)\n";

    const std::optional<meios::captured_diagnostic> first = window.first();
    if(first)
        std::cout << "  the first thing that went wrong was " << meios::to_string(first->code)
                  << " at " << meios::to_string(first->loc) << '\n';
    else
        std::cout << "  nothing in that window was an error\n";
}

}

int main()
{
    meios::log_sink_s stream(std::cerr);
    meios::capturing_log_sink capture(stream);

    const meios::capture_window flawed_window(capture);
    load_flawed(flawed_window);

    const meios::capture_window clean_window(capture);
    load_through(clean_window, examples::robot_description);

    report("the description naming a package that is not installed", flawed_window);
    report("the description that resolves completely", clean_window);

    if(flawed_window.records().empty())
    {
        std::cout << "the window holds nothing, so this run captured nothing to show\n";
        return 1;
    }

    return 0;
}
