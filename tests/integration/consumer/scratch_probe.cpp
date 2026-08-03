#include "consumer_probe.h"

#include <meios/io.h>
#include <meios/model.h>

#include <string>
#include <vector>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <filesystem>
#include <system_error>

namespace consumer
{

namespace
{

struct sink_probe
{
    sink_probe()
            : diagnostics()
            , sink(diagnostic_lift{diagnostics})
    {
    }

    std::vector<diagnostic_record> diagnostics;
    meios::log_sink_f<diagnostic_lift> sink;
};

int refuse(const char *what)
{
    std::cerr << what << '\n';
    return 1;
}

std::string read_file(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

std::optional<std::filesystem::path> resolve(meios::memory_source &source, const char *relative)
{
    const std::optional<meios::resolved_asset> hit = source.locate("pkg", relative);
    if(!hit)
        return std::nullopt;
    return hit->path();
}

std::optional<meios::operation_failure> cause_after(const std::vector<diagnostic_record> &records, std::size_t from)
{
    for(std::size_t i = records.size(); i > from; --i)
        if(records[i - 1].cause)
            return records[i - 1].cause;
    return std::nullopt;
}

int run_refusal(sink_probe &probe)
{
    meios::memory_source source{probe.sink};
    source.add("pkg", "meshes/arm.dae", "first-bytes");
    const std::optional<std::filesystem::path> located = resolve(source, "meshes/arm.dae");
    if(!located || read_file(*located) != "first-bytes")
        return refuse("a byte-backed source did not serve the bytes it was offered");

    const std::size_t before = probe.diagnostics.size();
    source.add("pkg", "meshes/arm.dae", "second-bytes");
    const std::optional<std::filesystem::path> again = resolve(source, "meshes/arm.dae");
    if(probe.diagnostics.size() == before)
        return refuse("a duplicate offer was accepted silently by a source built to reject");
    if(!again || *again != *located || read_file(*again) != "first-bytes")
        return refuse("a refused duplicate did not leave the first bytes resolving");
    std::cout << "the installed byte-backed source refused a duplicate key\n";
    return 0;
}

int run_replacement(sink_probe &probe)
{
    meios::memory_source source{probe.sink, meios::update_behavior::replace};
    source.add("pkg", "meshes/arm.dae", "first-bytes");
    const std::optional<std::filesystem::path> before = resolve(source, "meshes/arm.dae");
    if(!before)
        return refuse("a source built to replace could not resolve its first offer");

    source.add("pkg", "meshes/arm.dae", "second-bytes");
    const std::optional<std::filesystem::path> after = resolve(source, "meshes/arm.dae");
    if(!after || *after != *before || read_file(*after) != "second-bytes")
        return refuse("a replacement did not publish to the path a resolution already handed out");

    std::error_code ec;
    std::filesystem::remove(*after, ec);
    const std::optional<std::filesystem::path> again = resolve(source, "meshes/arm.dae");
    if(!again || *again != *before || read_file(*again) != "second-bytes")
        return refuse("an entry deleted outside the source was not rematerialized at its own path");
    std::cout << "the installed byte-backed source replaced and rematerialized one stable path\n";
    return 0;
}

// The provocation is a target whose parent is a regular file, so creating the parent directories
// fails. It needs no privilege and no platform guard, which is why the structured cause is
// observed on every platform rather than on whichever one happens to express a denial. The
// enumerator and the native value are printed rather than pinned: what each platform reports for
// this call has been measured on the development host alone.
int run_cause(sink_probe &probe)
{
    meios::memory_source source{probe.sink};
    source.add("pkg", "a", "leaf-bytes");
    source.add("pkg", "a/b", "beneath-a-regular-file");
    if(!resolve(source, "a"))
        return refuse("the leaf entry the publication failure depends on did not resolve");

    const std::size_t before = probe.diagnostics.size();
    if(resolve(source, "a/b"))
        return refuse("a publication whose parent is a regular file was not refused");
    const std::optional<meios::operation_failure> cause = cause_after(probe.diagnostics, before);
    if(!cause)
        return refuse("no cause-carrying diagnostic crossed the install boundary");
    std::cout << "a cause-carrying publication failure crossed the install boundary (operation=" << meios::to_string(cause->operation) << ", native=" << cause->native.value()
              << ", message=" << cause->native.message() << ")\n";
    return 0;
}

int run_teardown(sink_probe &probe)
{
    std::filesystem::path root;
    {
        meios::memory_source source{probe.sink};
        source.add("pkg", "meshes/arm.dae", "first-bytes");
        const std::optional<meios::resolved_asset> hit = source.locate("pkg", "meshes/arm.dae");
        if(!hit || !hit->source_root())
            return refuse("a resolution did not report the scratch root it was served from");
        root = *hit->source_root();
    }
    if(std::filesystem::exists(root))
        std::cout << "the scratch tree outlived its source, which a platform that will not remove"
                  << " a file somebody holds open is documented to leave behind\n";
    else
        std::cout << "the scratch tree went away with its source\n";
    return 0;
}

}

int run_memory_source()
{
    sink_probe probe;
    if(const int rc = run_refusal(probe))
        return rc;
    if(const int rc = run_replacement(probe))
        return rc;
    if(const int rc = run_cause(probe))
        return rc;
    if(const int rc = run_teardown(probe))
        return rc;
    std::cout << "exercised the installed byte-backed source over seven steps"
              << " (diagnostics=" << probe.diagnostics.size() << ", causes carried=" << (cause_after(probe.diagnostics, 0) ? 1 : 0) << ")\n";
    return 0;
}

}
