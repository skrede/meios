#include <meios/ros/ros_package_source.h>

#include <string>
#include <vector>
#include <cstddef>
#include <iostream>
#include <optional>
#include <filesystem>
#include <string_view>

namespace
{

std::filesystem::path runtime_dir(const char *argv0)
{
    const std::filesystem::path self(argv0);
    return self.has_parent_path() ? self.parent_path() : std::filesystem::current_path();
}

bool discovered(const std::vector<std::string> &packages, const std::filesystem::path &root)
{
    for(const std::string &name : packages)
        std::cout << "discovered " << name << '\n';
    if(!packages.empty())
        return true;
    std::cout << "no package manifest was discovered under " << root << '\n';
    return false;
}

bool resolved(meios::ros_package_source &source, std::string_view package, std::string_view relative)
{
    const std::optional<meios::resolved_asset> asset = source.locate(package, relative);
    if(!asset)
    {
        std::cout << "package://" << package << '/' << relative << " did not resolve\n";
        return false;
    }
    std::cout << "package://" << package << '/' << relative << " -> " << asset->path() << '\n';
    return true;
}

bool requested(meios::ros_package_source &source, std::string_view request)
{
    const std::size_t split = request.find('=');
    if(split == std::string_view::npos)
    {
        std::cout << "a request reads <package>=<relative>, not \"" << request << "\"\n";
        return false;
    }
    return resolved(source, request.substr(0, split), request.substr(split + 1));
}

}

int main(int argc, char **argv)
{
    meios::log_sink_s log(std::cout);
    const std::filesystem::path root = runtime_dir(argv[0]) / "ros_packages";

    meios::ros_package_source source({root}, {}, log);
    bool complete = discovered(source.packages(), root);
    if(argc < 2)
    {
        std::cout << "no reference was requested, so nothing was resolved\n";
        complete = false;
    }

    for(int index = 1; index < argc; ++index)
        complete = requested(source, argv[index]) && complete;

    return complete ? 0 : 1;
}
