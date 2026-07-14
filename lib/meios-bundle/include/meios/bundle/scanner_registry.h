#ifndef HPP_GUARD_MEIOS_BUNDLE_SCANNER_REGISTRY_H
#define HPP_GUARD_MEIOS_BUNDLE_SCANNER_REGISTRY_H

#include "meios/bundle/scanner_handle.h"

#include "meios/io/resolved_asset.h"

#include "meios/diagnostic/log_sink.h"

#include <string>
#include <vector>
#include <cctype>
#include <cstddef>
#include <utility>
#include <optional>
#include <string_view>
#include <unordered_map>

namespace meios
{

// Extension-keyed registry of type-erased scanners with a null default: an unknown
// extension returns {} rather than falling through to a default handler. The key is
// lowercase-normalized on both register and lookup so dispatch is case-insensitive.
class scanner_registry
{
public:
    scanner_registry &register_scanner(std::string extension, scanner_handle handle)
    {
        m_scanners.insert_or_assign(normalize(std::move(extension)), std::move(handle));
        return *this;
    }

    std::size_t size() const noexcept { return m_scanners.size(); }

    bool empty() const noexcept { return m_scanners.empty(); }

    bool contains(std::string_view extension) const
    {
        return m_scanners.contains(normalize(std::string(extension)));
    }

    std::vector<std::string> scan(std::string_view extension, const resolved_asset &asset, log_sink &log)
    {
        std::unordered_map<std::string, scanner_handle>::iterator hit =
            m_scanners.find(normalize(std::string(extension)));
        if(hit == m_scanners.end())
            return {};
        return hit->second.scan(asset, log);
    }

private:
    std::unordered_map<std::string, scanner_handle> m_scanners;

    static std::string normalize(std::string extension)
    {
        for(char &c : extension)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return extension;
    }
};

}

#endif
