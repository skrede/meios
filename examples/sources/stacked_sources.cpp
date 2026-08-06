#include <meios/io.h>

#include <meios/diagnostic/capturing_log_sink.h>

#include <string>
#include <cstddef>
#include <fstream>
#include <sstream>
#include <utility>
#include <iostream>
#include <optional>
#include <filesystem>
#include <string_view>
#include <system_error>

namespace
{

const char *const from_memory    = "<robot name=\"arm\"/> <!-- served from memory -->\n";
const char *const from_directory = "<robot name=\"arm\"/> <!-- served from the directory -->\n";

std::filesystem::path write_lower_layer()
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "meios_stacked_sources_example";
    std::error_code ec;
    std::filesystem::create_directories(root / "arm_description" / "urdf", ec);
    std::filesystem::create_directories(root / "arm_description" / "meshes", ec);
    std::ofstream(root / "arm_description" / "urdf" / "arm.urdf") << from_directory;
    std::ofstream(root / "arm_description" / "meshes" / "base.obj") << "v 0 0 0\n";
    return root;
}

std::string contents_of(const std::filesystem::path &path)
{
    std::ifstream in(path);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

std::string served(meios::source_stack &stack, std::string_view relative, meios::log_sink &log)
{
    const std::optional<meios::resolved_asset> found = stack.locate("arm_description", relative, log);
    if(!found)
        return {};
    std::cout << "package://arm_description/" << relative << " resolved to " << found->path() << '\n';
    return contents_of(found->path());
}

bool layered_as_specified(const std::string &contested, std::size_t shadows, const std::string &lower_only)
{
    if(contested != from_memory)
        std::cout << "the first layer did not win the reference both layers carry\n";
    else if(shadows == 0)
        std::cout << "nothing recorded that the second layer could also have answered\n";
    else if(lower_only.empty())
        std::cout << "the stack did not fall through to the layer only the second one carries\n";
    else
        return true;
    return false;
}

}

int main()
{
    const std::filesystem::path root = write_lower_layer();
    meios::log_sink_s stream(std::cout);
    meios::capturing_log_sink log(stream);

    meios::memory_source memory(log);
    memory.add("arm_description", "urdf/arm.urdf", from_memory);

    // Index 0 is the highest precedence, so the layer named first here is the layer that wins.
    meios::source_stack stack(std::move(memory), meios::directory_source(root, log));

    const std::size_t mark        = log.size();
    const std::string contested   = served(stack, "urdf/arm.urdf", log);
    const std::size_t shadows     = log.records(mark).size();
    const std::string lower_only  = served(stack, "meshes/base.obj", log);
    std::cout << "the contested lookup left " << shadows << " diagnostic(s) behind\n";

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    return layered_as_specified(contested, shadows, lower_only) ? 0 : 1;
}
