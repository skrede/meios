#include "fixture_path.h"
#include "consumer_probe.h"

#include <meios/urdf.h>
#include <meios/model.h>

#include <vector>
#include <iostream>

namespace consumer
{

namespace
{

int check_success(record_counter &sink, const meios::load_summary &summary)
{
    const bool counted = sink.finished && sink.robots == 1 && sink.links == 3 && sink.joints == 2;
    if(!counted)
    {
        std::cerr << "the facade drove the out-of-tree sink to unexpected counts:"
                  << " robots=" << sink.robots << " links=" << sink.links
                  << " joints=" << sink.joints << " finished=" << sink.finished << '\n';
        return 1;
    }
    if(!has(summary.claims, meios::completeness::parsed)
       || !has(summary.claims, meios::completeness::topology_valid))
    {
        std::cerr << "the summary withheld a claim the vendored description earns\n";
        return 1;
    }
    if(has(summary.claims, meios::completeness::deployment_complete))
    {
        std::cerr << "the summary claimed deployment completeness despite an undefined material\n";
        return 1;
    }
    return 0;
}

int check_untouched_on_failure()
{
    std::vector<diagnostic_record> diagnostics;
    meios::log_sink_f diagnostic_sink{ diagnostic_lift{ diagnostics } };

    record_counter sink;
    const meios::load_summary summary =
        meios::load_into("no_such_description.urdf", meios::load_options{}, sink, diagnostic_sink);

    if(sink.robots != 0 || sink.materials != 0 || sink.links != 0 || sink.joints != 0
       || sink.finished)
    {
        std::cerr << "a failed load reached the out-of-tree sink:"
                  << " robots=" << sink.robots << " materials=" << sink.materials
                  << " links=" << sink.links << " joints=" << sink.joints
                  << " finished=" << sink.finished << '\n';
        return 1;
    }
    if(summary.claims != meios::completeness::none)
    {
        std::cerr << "a failed load still claimed something\n";
        return 1;
    }
    return 0;
}

}

int run_facade()
{
    std::vector<diagnostic_record> diagnostics;
    meios::log_sink_f diagnostic_sink{ diagnostic_lift{ diagnostics } };

    meios::load_options opts;
    opts.materials = meios::material_policy::warn;

    record_counter sink;
    const meios::load_summary summary =
        meios::load_into(vendored_robot, opts, sink, diagnostic_sink);

    if(const int rc = check_success(sink, summary))
        return rc;
    if(const int rc = check_untouched_on_failure())
        return rc;

    std::cout << "drove an out-of-tree sink from a path through the installed facade"
              << " (links=" << sink.links << ", joints=" << sink.joints
              << ", diagnostics=" << summary.diagnostics.size() << ")\n";
    return 0;
}

}
