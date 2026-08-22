#include <meios/io.h>

#include <meios/diagnostic/capturing_log_sink.h>

#include <string>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <optional>
#include <filesystem>
#include <string_view>
#include <system_error>

namespace
{

std::filesystem::path write_package()
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "meios_directory_source_example";
    std::error_code ec;
    std::filesystem::create_directories(root / "arm_description" / "urdf", ec);
    std::filesystem::create_directories(root / "arm_description" / "meshes", ec);
    std::ofstream(root / "arm_description" / "urdf" / "arm.urdf")
        << "<robot name=\"arm\"><link name=\"base_link\"/></robot>\n";
    std::ofstream(root / "arm_description" / "meshes" / "base.obj") << "v 0 0 0\n";
    return root;
}

void report(const meios::source_lookup_result &result, std::string_view what)
{
    if(!result)
        std::cout << what << ": the lookup could not be carried out -- "
                  << meios::to_string(result.error().operation) << " failed with "
                  << result.error().native.message() << '\n';
    else if(!*result)
        std::cout << what << ": no asset\n";
    else
        std::cout << what << ": " << (*result)->path() << '\n';
}

std::size_t errors_from(meios::directory_source &source, std::string_view relative, meios::capturing_log_sink &log, std::string_view what)
{
    const std::size_t mark = log.size();
    report(source.try_locate("arm_description", relative), what);
    return log.errors(mark);
}

bool resolved_to_a_real_file(const std::optional<meios::resolved_asset> &found)
{
    if(!found)
        return false;
    std::cout << "package://arm_description/urdf/arm.urdf resolved to " << found->path() << '\n';
    std::error_code ec;
    return std::filesystem::exists(found->path(), ec);
}

bool resolved_as_specified(bool on_disk, std::size_t missed, std::size_t refused)
{
    if(!on_disk)
        std::cout << "the entry that exists did not resolve to a file that is really there\n";
    else if(missed != 0 || refused == 0)
        std::cout << "a miss and a refusal came back indistinguishable\n";
    else
        return true;
    return false;
}

}

int main()
{
    const std::filesystem::path root = write_package();
    meios::log_sink_s stream(std::cout);
    meios::capturing_log_sink log(stream);

    meios::directory_source source(root, log);
    const bool on_disk = resolved_to_a_real_file(source.locate("arm_description", "urdf/arm.urdf"));
    const std::size_t missed = errors_from(source, "urdf/gripper.urdf", log, "an entry the package does not hold");

    // A source configured with no root contains nothing, so an ordinary reference is refused rather
    // than merely missed. Both hand back an empty asset; what tells them apart is that one of them
    // said why.
    meios::directory_source unrooted(std::filesystem::path{}, log);
    const std::size_t refused = errors_from(unrooted, "urdf/arm.urdf", log, "the same entry through a source given no root");
    std::cout << "the miss was reported " << missed << " time(s), the refusal " << refused << " time(s)\n";

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    return resolved_as_specified(on_disk, missed, refused) ? 0 : 1;
}
