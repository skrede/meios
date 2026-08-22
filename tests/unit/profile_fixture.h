#ifndef HPP_GUARD_MEIOS_UNIT_PROFILE_FIXTURE_H
#define HPP_GUARD_MEIOS_UNIT_PROFILE_FIXTURE_H

#include <meios/urdf.h>
#include <meios/model.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/completeness.h>
#include <meios/diagnostic/source_location.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <string>
#include <vector>
#include <cstddef>
#include <utility>
#include <algorithm>
#include <filesystem>

namespace profile
{

struct captured
{
    meios::level           lvl;
    meios::diagnostic_code code;
    meios::source_location loc;
};

struct recorder
{
    std::vector<captured> &sink;

    void operator()(meios::level lvl, const std::string &)
    {
        sink.push_back({ lvl, meios::diagnostic_code::unspecified, {} });
    }

    void operator()(meios::level lvl, const meios::source_location &loc, const std::string &)
    {
        sink.push_back({ lvl, meios::diagnostic_code::unspecified, loc });
    }

    void operator()(meios::level lvl, meios::diagnostic_code code, const meios::source_location &loc,
                    const std::string &)
    {
        sink.push_back({ lvl, code, loc });
    }
};

struct outcome
{
    std::vector<captured> diagnostics;
    bool                  succeeded;
    meios::completeness   claims;
    meios::model<double>  robot;
};

inline outcome load_fixture(const std::string &fixture, const meios::load_options &opts = {})
{
    std::vector<captured> out;
    meios::log_sink_f capture{ recorder{ out } };
    const std::filesystem::path path =
        std::filesystem::path{ MEIOS_URDF_FIXTURE_DIR } / "profile" / fixture;
    meios::expected<meios::load_result, meios::load_error> loaded =
        meios::load(path, opts, capture);
    if(!loaded)
        return { std::move(out), false, meios::completeness::none, {} };
    return { std::move(out), true, loaded->claims, std::move(loaded->robot) };
}

inline bool located(const std::vector<captured> &diagnostics, meios::level lvl,
                    const std::string &code)
{
    return std::any_of(diagnostics.begin(), diagnostics.end(), [lvl, &code](const captured &d) {
        return d.lvl == lvl && meios::to_string(d.code) == code && !d.loc.file.empty()
            && d.loc.line > 0;
    });
}

inline meios::diagnostic_code code_from(const std::string &name)
{
    for(int value = 0; meios::to_string(static_cast<meios::diagnostic_code>(value)) != "unknown";
        ++value)
    {
        const meios::diagnostic_code code = static_cast<meios::diagnostic_code>(value);
        if(meios::to_string(code) == name)
            return code;
    }
    return meios::diagnostic_code::unspecified;
}

inline std::size_t errors(const std::vector<captured> &diagnostics)
{
    return static_cast<std::size_t>(
        std::count_if(diagnostics.begin(), diagnostics.end(),
                      [](const captured &d) { return d.lvl == meios::level::error; }));
}

}

#endif
