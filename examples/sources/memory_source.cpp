#include <meios/io.h>

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <optional>
#include <filesystem>

namespace
{

const char *const description = "<robot name=\"arm\"><link name=\"base_link\"/></robot>\n";

std::string contents_of(const std::filesystem::path &path)
{
    std::ifstream in(path);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

bool reads_back_what_was_added(const std::optional<meios::resolved_asset> &found)
{
    if(!found)
        return false;
    std::cout << "package://arm_description/urdf/arm.urdf resolved to " << found->path() << '\n';
    return contents_of(found->path()) == description;
}

bool served_as_specified(bool round_tripped, bool never_added_resolved)
{
    if(!round_tripped)
        std::cout << "the resolved path did not read back the bytes that were added\n";
    else if(never_added_resolved)
        std::cout << "an entry that was never added resolved anyway\n";
    else
        return true;
    return false;
}

}

int main()
{
    meios::log_sink_s log(std::cout);
    meios::memory_source source(log);
    source.add("arm_description", "urdf/arm.urdf", description)
        .add("arm_description", "meshes/base.obj", "v 0 0 0\n");

    const bool round_tripped = reads_back_what_was_added(source.locate("arm_description", "urdf/arm.urdf"));
    const std::optional<meios::resolved_asset> never_added = source.locate("arm_description", "urdf/gripper.urdf");
    std::cout << "an entry that was never added resolves to " << (never_added ? "a path" : "nothing") << '\n';

    return served_as_specified(round_tripped, never_added.has_value()) ? 0 : 1;
}
