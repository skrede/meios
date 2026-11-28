#ifndef HPP_GUARD_MEIOS_UNIT_BUNDLE_SCANNER_FIXTURE_H
#define HPP_GUARD_MEIOS_UNIT_BUNDLE_SCANNER_FIXTURE_H

#include <meios/io.h>
#include <meios/model.h>
#include <meios/bundle.h>

#include <string>
#include <vector>
#include <utility>
#include <optional>

namespace
{

struct obj_scanner
{
    std::vector<std::string> refs;

    std::vector<std::string> scan(const meios::resolved_asset &, meios::log_sink &)
    {
        return refs;
    }
};

inline meios::reference_record mesh_ref(std::string original, std::optional<std::string> resolved)
{
    return meios::reference_record{std::move(original), std::move(resolved), false};
}

}

#endif
