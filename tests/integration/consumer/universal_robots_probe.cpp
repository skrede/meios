#include "model_facts.h"
#include "corpus_paths.h"

#include <meios/urdf.h>
#include <meios/model.h>

#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <exception>

namespace
{

int refuse_load(const meios::load_error &error)
{
    std::ostringstream refusal;
    refusal << "load() refused " << consumer::pinned_document << ": ("
            << meios::to_string(error.code) << ") " << error.message;
    return consumer::refuse(refusal.str());
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
        return refuse_load(loaded.error());

    const meios::model<double> &robot = loaded->robot;
    if(const int rc = consumer::check_all_facts(robot, rows))
        return rc;

    std::cout << "loaded " << consumer::pinned_document << " as " << consumer::variant
              << " (links=" << robot.links.size() << ", joints=" << robot.joints.size()
              << ") and matched " << rows.size() << " recorded facts\n";
    return 0;
}

}

int main()
{
    try
    {
        return load_and_compare(oracle::load_rows(consumer::pinned_record));
    }
    catch(const std::exception &unreadable)
    {
        return consumer::refuse(std::string("the committed record ") + consumer::pinned_record
                      + " yielded nothing to compare against: " + unreadable.what());
    }
}
