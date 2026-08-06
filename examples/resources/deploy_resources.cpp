#include <meios/urdf.h>
#include <meios/model.h>

#include <string>
#include <variant>
#include <cstddef>
#include <iostream>
#include <filesystem>

namespace
{

// The point of deploying a resource beside the executable is that the runtime path is derived from
// the binary, not baked in at configure time, so the build tree can be relocated or zipped up and
// handed out. argv[0] carries a full path under CTest and under any normal launch of an installed
// app; the fallback keeps a bare-name invocation from resolving against the filesystem root.
std::filesystem::path runtime_dir(const char *argv0)
{
    const std::filesystem::path self(argv0);
    return self.has_parent_path() ? self.parent_path() : std::filesystem::current_path();
}

}

int main(int, char **argv)
{
    const std::filesystem::path package_root = runtime_dir(argv[0]) / "urdf";
    const std::filesystem::path description = package_root / "example_arm" / "urdf" / "arm.urdf";

    std::cout << "package root: " << package_root.string() << '\n';

    meios::load_options options;
    options.package_roots.push_back(package_root);

    const meios::expected<meios::load_result, meios::load_error> loaded =
        meios::load(description, options);
    if(!loaded)
    {
        std::cout << "load failed: " << loaded.error().message << '\n';
        return 1;
    }

    const meios::model<double> &model = loaded->robot;
    std::cout << "loaded \"" << model.name << "\" with " << model.links.size() << " links\n";

    for(const meios::link<double> &part : model.links)
    {
        for(const meios::visual<double> &surface : part.visuals)
        {
            const meios::mesh<double> *geometry = std::get_if<meios::mesh<double>>(&surface.geom.shape);
            if(geometry == nullptr)
                continue;

            std::cout << "  " << part.name << ": " << geometry->filename << '\n';
            if(geometry->resolved_path)
                std::cout << "    resolved to " << *geometry->resolved_path << '\n';
            else
                std::cout << "    unresolved\n";
        }
    }

    return 0;
}
