#include "kr6_paths.h"
#include "model_facts.h"
#include "interpreter_absence.h"

#include <meios/urdf.h>
#include <meios/model.h>

#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <exception>

namespace
{

int load_and_compare(const std::vector<oracle::row> &rows)
{
    meios::load_options opts;
    opts.on_missing = meios::missing_asset::warn;
    opts.materials = meios::material_policy::warn;
    opts.package_roots.push_back(consumer::pinned_package_root);

    const meios::expected<meios::load_result, meios::load_error> loaded =
        meios::load(consumer::pinned_document, opts);
    if(!loaded)
    {
        std::ostringstream refusal;
        refusal << "load() refused " << consumer::pinned_document << ": ("
                << meios::to_string(loaded.error().code) << ") " << loaded.error().message;
        return consumer::refuse(refusal.str());
    }

    const meios::model<double> &robot = loaded->robot;
    if(const int rc = consumer::check_structure(robot, rows))
        return rc;
    if(const int rc = consumer::check_limits(robot, rows))
        return rc;
    if(const int rc = consumer::check_assets(robot, rows))
        return rc;

    std::cout << "loaded " << consumer::pinned_document
              << " (links=" << robot.links.size() << ", joints=" << robot.joints.size()
              << ") and matched " << rows.size() << " recorded facts\n";
    return 0;
}

}

int main(int argc, char **argv)
{
    if(const int rc = consumer::guard_interpreter_absence(argc, argv))
        return rc;

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
