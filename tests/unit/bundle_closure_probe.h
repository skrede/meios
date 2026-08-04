#ifndef HPP_GUARD_MEIOS_UNIT_BUNDLE_CLOSURE_PROBE_H
#define HPP_GUARD_MEIOS_UNIT_BUNDLE_CLOSURE_PROBE_H

// A claim that a reference is never re-resolved needs a source that counts being asked rather
// than one that only answers, two parents naming the one child so a second lookup is reachable
// at all, and a closure whose parents and child share one package root — the collision rule
// would otherwise refuse a child before the resolution under test is reached.

#include "asset_read_probe.h"

#include <meios/bundle.h>

#include <meios/io/source_stack.h>
#include <meios/io/source_lookup.h>
#include <meios/io/resolved_asset.h>
#include <meios/io/package_source.h>

#include <meios/diagnostic/log_sink.h>

#include <map>
#include <string>
#include <vector>
#include <fstream>
#include <optional>
#include <filesystem>
#include <functional>
#include <string_view>

namespace asset_probe
{

struct read_probe
{
    read_probe() : records(), log(recorder{ records }) {}

    std::vector<captured> records;
    meios::log_sink_f<recorder> log;
};

inline void write_file(const std::filesystem::path &path, const std::string &text)
{
    std::ofstream out(path, std::ios::binary);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
}

// The counter sits at the entry of both lookup members, so a change that merely swapped the
// untyped lookup for the typed one inside the builder would still be visible here.
struct counting_source
{
    std::map<std::string, std::filesystem::path> table;
    std::reference_wrapper<int> calls;

    meios::capability_descriptor capabilities() const
    {
        return meios::capability_descriptor{ meios::source_kind::directory, false };
    }

    std::optional<meios::resolved_asset> locate(std::string_view pkg, std::string_view rel)
    {
        ++calls.get();
        return seeded(pkg, rel);
    }

    meios::source_lookup_result try_locate(std::string_view pkg, std::string_view rel)
    {
        ++calls.get();
        return seeded(pkg, rel);
    }

    std::optional<meios::resolved_asset> seeded(std::string_view pkg, std::string_view rel) const
    {
        const std::map<std::string, std::filesystem::path>::const_iterator hit = table.find(std::string(pkg) + '/' + std::string(rel));
        if(hit == table.end())
            return std::nullopt;
        return meios::resolved_asset{ hit->second };
    }
};

struct child_scanner
{
    std::vector<std::string> refs;
    std::reference_wrapper<int> dispatches;

    std::vector<std::string> scan(const meios::resolved_asset &, meios::log_sink &)
    {
        ++dispatches.get();
        return refs;
    }
};

struct package_tree
{
    explicit package_tree(const std::filesystem::path &root) : child(root / "ur5" / "materials" / "wood.mtl"), parent(root / "ur5" / "meshes" / "base.obj"), sibling(root / "ur5" / "meshes" / "tool.obj")
    {
        std::filesystem::create_directories(child.parent_path());
        write_file(parent, "");
        write_file(sibling, "");
    }

    std::filesystem::path child;
    std::filesystem::path parent;
    std::filesystem::path sibling;
};

inline meios::scanner_registry closure_registry(int &dispatches)
{
    meios::scanner_registry registry;
    registry.register_scanner("obj", meios::scanner_handle{ child_scanner{ { "package://ur5/materials/wood.mtl" }, dispatches } });
    registry.register_scanner("mtl", meios::scanner_handle{ child_scanner{ {}, dispatches } });
    return registry;
}

}

#endif
