#include <meios/io.h>
#include <meios/urdf.h>
#include <meios/bundle.h>

#include <string>
#include <utility>
#include <iostream>
#include <optional>
#include <filesystem>

namespace
{

constexpr const char *arm_urdf =
    "<?xml version=\"1.0\"?>\n"
    "<robot name=\"arm\">\n"
    "  <link name=\"base_link\">\n"
    "    <visual>\n"
    "      <geometry><mesh filename=\"package://arm_description/meshes/base.stl\"/></geometry>\n"
    "    </visual>\n"
    "  </link>\n"
    "</robot>\n";

meios::source_stack supplied_package(meios::log_sink &log)
{
    meios::memory_source package(log);
    package.add("arm_description", "urdf/arm.urdf", arm_urdf)
        .add("arm_description", "meshes/base.stl", "solid base\nendsolid base\n");
    return meios::source_stack(std::move(package));
}

meios::asset_manifest would_be_written(const meios::model<double> &robot, meios::source_stack &sources,
                                       meios::emit_result &out, meios::log_sink &log)
{
    meios::scanner_registry registry;
    const meios::folder_request request{std::filesystem::temp_directory_path() / "meios_would_be_bundle",
                                        "arm_bundle", meios::collision_options{false}, true};
    return meios::bundle_to_folder(robot, sources, registry, request, out, log);
}

void report(const meios::emit_result &out, const meios::asset_manifest &manifest)
{
    std::cout << "a folder bundle would have been written with status " << meios::to_string(out.status)
              << ": " << manifest.entries.size() << " entry(ies), " << manifest.unresolved.size()
              << " reference(s) it could not resolve\n";
    for(const meios::bundle_entry &entry : manifest.entries)
        std::cout << "  " << entry.dest_relative << '\n';
    for(const std::string &missing : manifest.unresolved)
        std::cout << "  unresolved: " << missing << '\n';
}

bool would_have_bundled(const meios::model<double> &robot, meios::source_stack &sources, meios::log_sink &log)
{
    meios::emit_result out{};
    const meios::asset_manifest manifest = would_be_written(robot, sources, out, log);
    report(out, manifest);
    if(out.status != meios::emit_status::ok)
        std::cout << "the bundler did not report success\n";
    else if(manifest.entries.empty())
        std::cout << "the bundle would have contained nothing\n";
    else
        return true;
    return false;
}

}

int main()
{
    meios::log_sink_s stream(std::cout);
    meios::capturing_log_sink log(stream);

    meios::source_stack sources = supplied_package(log);
    const std::optional<meios::resolved_asset> description =
        sources.locate("arm_description", "urdf/arm.urdf", log);
    if(!description)
    {
        std::cout << "the description was never supplied\n";
        return 1;
    }

    const meios::expected<meios::load_result, meios::load_error> loaded =
        meios::load(description->path(), {}, sources, log);
    if(!loaded)
    {
        std::cout << "load failed: " << loaded.error().message << '\n';
        return 1;
    }

    return would_have_bundled(loaded->robot, sources, log) ? 0 : 1;
}
