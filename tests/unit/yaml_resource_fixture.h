#ifndef HPP_GUARD_MEIOS_TEST_YAML_RESOURCE_FIXTURE_H
#define HPP_GUARD_MEIOS_TEST_YAML_RESOURCE_FIXTURE_H

#include "meios/urdf/yaml_resource.h"

#include <meios/io/source_stack.h>
#include <meios/io/source_handle.h>
#include <meios/io/directory_source.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>

#include <random>
#include <string>
#include <vector>
#include <fstream>
#include <utility>
#include <filesystem>
#include <string_view>

namespace yaml_resource_test
{

struct tally
{
    int errors{0};
    std::vector<std::string> messages{};

    void operator()(meios::level lvl, const std::string &message)
    {
        if(lvl == meios::level::error)
            ++errors;
        messages.push_back(message);
    }

    void operator()(meios::level lvl, const meios::source_location &, const std::string &message)
    {
        (*this)(lvl, message);
    }
};

inline bool says(const tally &counts, std::string_view fragment)
{
    for(const std::string &message : counts.messages)
        if(message.find(fragment) != std::string::npos)
            return true;
    return false;
}

// Owns everything the loader captures by reference, so a case's loader stays valid for
// exactly as long as the case does.
struct probe
{
    probe(meios::log_sink &log, const std::filesystem::path &root, std::vector<std::filesystem::path> configured)
            : sources()
            , roots(std::move(configured))
            , loader(meios::detail::make_yaml_text_loader(sources, roots, log))
    {
        if(!root.empty())
            sources.push_back(meios::source_handle(meios::directory_source(root, log)));
    }

    meios::source_stack sources;
    std::vector<std::filesystem::path> roots;
    meios::text_resource_loader loader;
};

inline std::filesystem::path fresh_dir()
{
    std::random_device device;
    for(;;)
    {
        std::filesystem::path candidate = std::filesystem::temp_directory_path() / ("meios-yaml-" + std::to_string(device()));
        if(!std::filesystem::exists(candidate))
            return candidate;
    }
}

// pkg/ is the package a source layer serves; docs/ and other/ are two documents holding
// same-named neighbours, and outside/ is the target a link must not be able to reach.
inline std::filesystem::path seed_tree()
{
    std::filesystem::path root = fresh_dir();
    std::filesystem::create_directories(root / "pkg");
    std::filesystem::create_directories(root / "docs" / "sub");
    std::filesystem::create_directories(root / "other");
    std::filesystem::create_directories(root / "outside");
    std::ofstream(root / "pkg" / "cfg.yaml") << "from: package";
    std::ofstream(root / "docs" / "cfg.yaml") << "from: docs";
    std::ofstream(root / "docs" / "sub" / "cfg.yaml") << "from: docs-sub";
    std::ofstream(root / "other" / "cfg.yaml") << "from: other";
    std::ofstream(root / "outside" / "secret.yaml") << "from: outside";
    std::ofstream(root / "loose.yaml") << "from: root";
    return root;
}

}

#endif
