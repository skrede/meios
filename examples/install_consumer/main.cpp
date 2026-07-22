#include <meios/urdf.h>

#include <cstddef>
#include <iostream>
#include <filesystem>

namespace
{

// Deriving the root from argv[0] rather than baking a configure-time path in is what lets the same
// binary work out of the build tree and out of the install tree; INSTALL_RUNTIME_RELATIVE exists to
// keep that one lookup valid in both. argv[0] carries a full path under CTest and under a normal
// launch of an installed program; the fallback keeps a bare-name invocation from resolving against
// the filesystem root.
std::filesystem::path runtime_dir(const char *argv0)
{
    const std::filesystem::path self(argv0);
    return self.has_parent_path() ? self.parent_path() : std::filesystem::current_path();
}

}

int main(int, char **argv)
{
    const std::filesystem::path package_root = runtime_dir(argv[0]) / "urdf";

    meios::load_options options;
    options.package_roots.push_back(package_root);

    const auto robot = meios::load(package_root / "example_arm" / "urdf" / "arm.urdf", options);
    if (!robot)
    {
        std::cout << "load failed at " << meios::to_string(robot.error().loc) << ": "
                  << robot.error().message << '\n';
        return 1;
    }

    std::cout << "resolved \"" << robot->name << "\" from " << package_root.string() << '\n';
    std::cout << "links: " << robot->links.size() << '\n';

    return 0;
}
