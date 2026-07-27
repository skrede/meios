#ifndef HPP_GUARD_MEIOS_UNIT_FIELD_PROBE_H
#define HPP_GUARD_MEIOS_UNIT_FIELD_PROBE_H

#include <meios/urdf.h>
#include <meios/model.h>
#include <meios/io.h>
#include <meios/xacro.h>

#include <meios/diagnostic/diagnostic_code.h>

#include <string>
#include <vector>
#include <cstddef>
#include <algorithm>
#include <filesystem>

namespace probe
{

struct note
{
    meios::level           lvl;
    meios::diagnostic_code code;
    std::string            message;
};

struct recorder
{
    std::vector<note> &sink;

    void operator()(meios::level lvl, const std::string &message)
    {
        sink.push_back({ lvl, meios::diagnostic_code::unspecified, message });
    }

    void operator()(meios::level lvl, const meios::source_location &, const std::string &message)
    {
        sink.push_back({ lvl, meios::diagnostic_code::unspecified, message });
    }

    void operator()(meios::level lvl, meios::diagnostic_code code, const meios::source_location &,
                    const std::string &message)
    {
        sink.push_back({ lvl, code, message });
    }
};

struct outcome
{
    std::vector<note>   notes;
    meios::tree<double> robot;
};

inline std::string document(const std::string &body)
{
    return "<?xml version=\"1.0\"?>\n<robot name=\"fields\">\n" + body + "</robot>\n";
}

// The graph policy is set aside so a rule that drops or refuses one element is observed on
// its own rather than through the extra root the missing edge would leave behind.
inline outcome walk(const std::string &body, meios::strictness strict)
{
    outcome out{};
    meios::log_sink_f capture{ recorder{ out.notes } };
    meios::source_stack sources{};
    meios::core_evaluator eval;
    meios::parse_context ctx{ sources, eval, capture, meios::missing_asset::warn,
                              meios::topology_policy::skip, meios::material_policy::warn, strict,
                              "fields.urdf" };
    meios::pod_recorder<meios::tree<double>> rec(capture, meios::topology_policy::skip);
    meios::basic_parser<meios::urdf_reader> parser(ctx);
    parser.parse(document(body), rec);
    out.robot = rec.result();
    return out;
}

inline bool carries(const outcome &out, meios::level lvl, meios::diagnostic_code code)
{
    return std::any_of(out.notes.begin(), out.notes.end(),
                       [lvl, code](const note &n) { return n.lvl == lvl && n.code == code; });
}

inline bool mentions(const outcome &out, meios::diagnostic_code code)
{
    return std::any_of(out.notes.begin(), out.notes.end(),
                       [code](const note &n) { return n.code == code; });
}

inline std::size_t errors(const outcome &out)
{
    return static_cast<std::size_t>(
        std::count_if(out.notes.begin(), out.notes.end(),
                      [](const note &n) { return n.lvl == meios::level::error; }));
}

inline std::string message_for(const outcome &out, meios::diagnostic_code code)
{
    for(const note &n : out.notes)
        if(n.code == code)
            return n.message;
    return {};
}

inline std::filesystem::path fixture(const std::string &name)
{
    return std::filesystem::path{ MEIOS_URDF_FIXTURE_DIR } / "profile" / name;
}

}

#endif
