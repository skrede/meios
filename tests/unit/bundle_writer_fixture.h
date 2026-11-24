#ifndef HPP_GUARD_MEIOS_UNIT_BUNDLE_WRITER_FIXTURE_H
#define HPP_GUARD_MEIOS_UNIT_BUNDLE_WRITER_FIXTURE_H

#include <meios/io.h>
#include <meios/urdf.h>
#include <meios/model.h>
#include <meios/xacro.h>

#include <string>
#include <vector>

namespace
{

struct capture_log : meios::log_sink
{
    using meios::log_sink::log;

    std::vector<meios::level> levels;

    void log(meios::level lvl, const std::string &) override
    {
        levels.push_back(lvl);
    }

    void log(meios::level lvl, const meios::source_location &, const std::string &) override
    {
        levels.push_back(lvl);
    }

    bool saw(meios::level want) const
    {
        for(meios::level lvl : levels)
            if(lvl == want)
                return true;
        return false;
    }
};

inline meios::tree<double> reparse(const std::string &text, meios::log_sink &log)
{
    meios::source_stack sources{};
    meios::core_evaluator eval;
    meios::parse_context ctx{sources, eval, log, meios::missing_asset::warn, meios::topology_policy::fail, meios::material_policy::warn, meios::strictness::fail, "emit.urdf"};
    meios::pod_recorder<meios::tree<double>> rec(log, meios::topology_policy::fail);
    meios::basic_parser<meios::urdf_reader> parser(ctx);
    parser.parse(text, rec);
    return rec.result();
}

}

#endif
