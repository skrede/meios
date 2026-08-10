#include "model_facts.h"
#include "corpus_paths.h"

#include <meios/urdf.h>
#include <meios/model.h>

#include <string>
#include <vector>
#include <cstddef>
#include <cstdlib>
#include <sstream>
#include <iostream>
#include <filesystem>

namespace
{

#ifdef _WIN32
constexpr char path_separator = ';';
#else
constexpr char path_separator = ':';
#endif

int refuse(const std::string &what)
{
    std::cerr << what << '\n';
    return 1;
}

// The interpreter-free registration empties the process path; without reading it back that
// registration would be an environment setting nobody checked, and a host carrying an interpreter
// beside the executable would satisfy it silently.
int refuse_reachable_interpreter()
{
    const char *path = std::getenv("PATH");
    for(std::string rest = path == nullptr ? std::string{} : path; !rest.empty();)
    {
        const std::size_t split_at = rest.find(path_separator);
        const std::string dir = rest.substr(0, split_at);
        rest = split_at == std::string::npos ? std::string{} : rest.substr(split_at + 1);
        if(dir.empty())
            continue;
        for(const char *name : { "python3", "python" })
            if(std::filesystem::exists(std::filesystem::path(dir) / name))
                return refuse(std::string("an interpreter is reachable at ") + dir + "/" + name
                              + ", so this run cannot state that the load needed none");
    }
    return 0;
}

int load_and_compare(const std::vector<oracle::row> &rows)
{
    meios::load_options opts;
    opts.on_missing = meios::missing_asset::warn;
    opts.materials = meios::material_policy::warn;
    opts.args.emplace(consumer::variant_argument, consumer::variant);
    opts.package_roots.push_back(consumer::pinned_package_root);

    const meios::expected<meios::load_result, meios::load_error> loaded =
        meios::load(consumer::pinned_document, opts);
    if(!loaded)
    {
        std::ostringstream refusal;
        refusal << "load() refused " << consumer::pinned_document << ": ("
                << meios::to_string(loaded.error().code) << ") " << loaded.error().message;
        return refuse(refusal.str());
    }

    const meios::model<double> &robot = loaded->robot;
    if(const int rc = consumer::check_structure(robot, rows))
        return rc;
    if(const int rc = consumer::check_limits(robot, rows))
        return rc;
    if(const int rc = consumer::check_assets(robot, rows))
        return rc;

    std::cout << "loaded " << consumer::pinned_document << " as " << consumer::variant
              << " (links=" << robot.links.size() << ", joints=" << robot.joints.size()
              << ") and matched " << rows.size() << " recorded facts\n";
    return 0;
}

}

int main()
{
    if(std::getenv("MEIOS_CONSUMER_NO_INTERPRETER") != nullptr)
        if(const int rc = refuse_reachable_interpreter())
            return rc;

    const std::vector<oracle::row> rows = oracle::load_rows(consumer::pinned_record);
    if(rows.empty())
        return refuse(std::string("the committed record ") + consumer::pinned_record
                      + " loaded no rows, so every comparison below would be vacuous");

    return load_and_compare(rows);
}
