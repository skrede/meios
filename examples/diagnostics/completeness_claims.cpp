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

// The undeclared material withholds one claim and the vendor extension block withholds none, so
// the two sources of the claim value below have something to agree about. Both are reported under
// the default policies; nothing here is loosened.
std::filesystem::path write_description()
{
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "meios_claims_example.urdf";
    std::ofstream out(path);
    out << "<?xml version=\"1.0\"?>\n"
           "<robot name=\"claims_arm\">\n"
           "  <link name=\"base_link\">\n"
           "    <visual><geometry><box size=\"1 1 1\"/></geometry>\n"
           "      <material name=\"never_declared\"/></visual>\n"
           "  </link>\n"
           "  <gazebo reference=\"base_link\"><mu1>0.2</mu1></gazebo>\n"
           "</robot>\n";
    return path;
}

meios::expected<meios::load_result, meios::load_error>
load_scratch(const meios::capture_window &window)
{
    const std::filesystem::path path = write_description();

    const meios::load_options options;
    meios::expected<meios::load_result, meios::load_error> loaded =
        meios::load(path, options, window.log());

    std::error_code ec;
    std::filesystem::remove(path, ec);

    return loaded;
}

void say(bool held, const char *sentence)
{
    std::cout << (held ? "  yes -- " : "  no  -- ") << sentence << '\n';
}

void report(meios::completeness claims)
{
    say(has(claims, meios::completeness::parsed),
        "the document was read and everything it declared reached the model");
    say(has(claims, meios::completeness::topology_valid),
        "the links and joints hang together as one tree with a single root");
    say(has(claims, meios::completeness::deployment_complete),
        "every mesh, texture and material the description named was found");
}

void compare(meios::completeness claimed, const std::vector<meios::captured_diagnostic> &records)
{
    if(meios::claims_from(records) == claimed)
        std::cout << "the captured diagnostics account for exactly the claims withheld\n";
    else
        std::cout << "the claims and the captured diagnostics disagree about this load\n";
}

}

int main()
{
    meios::log_sink_s stream(std::cerr);
    meios::capturing_log_sink capture(stream);
    const meios::capture_window window(capture);

    const meios::expected<meios::load_result, meios::load_error> loaded = load_scratch(window);
    if(!loaded)
    {
        std::cout << "the load failed, so it claims nothing at all\n";
        return 1;
    }

    report(loaded->claims);
    compare(loaded->claims, window.records());

    if(!has(loaded->claims, meios::completeness::parsed))
    {
        std::cout << "the load does not claim to have parsed, so nothing else it says holds\n";
        return 1;
    }

    return 0;
}
