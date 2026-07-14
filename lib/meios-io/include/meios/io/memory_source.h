#ifndef HPP_GUARD_MEIOS_IO_MEMORY_SOURCE_H
#define HPP_GUARD_MEIOS_IO_MEMORY_SOURCE_H

#include "meios/io/package_source.h"
#include "meios/io/resolved_asset.h"

#include <map>
#include <span>
#include <memory>
#include <string>
#include <cstring>
#include <utility>
#include <cstddef>
#include <optional>
#include <algorithm>
#include <string_view>

namespace meios
{

namespace detail
{

struct buffer_puller final : byte_reader::puller
{
    explicit buffer_puller(std::string bytes) : m_data(std::move(bytes)), m_pos(0) {}

    std::size_t read(std::span<std::byte> out) override
    {
        std::size_t n = std::min(out.size(), m_data.size() - m_pos);
        std::memcpy(out.data(), m_data.data() + m_pos, n);
        m_pos += n;
        return n;
    }

    std::string m_data;
    std::size_t m_pos;
};

}

// An in-memory package source keyed by (package, relative). It hands back only
// byte streams over the stored buffers, so it never satisfies provides_path.
class memory_source
{
public:
    memory_source() : m_entries() {}

    memory_source &add(std::string package, std::string relative, std::string bytes)
    {
        m_entries.insert_or_assign(key{ std::move(package), std::move(relative) },
                                   std::move(bytes));
        return *this;
    }

    capability_descriptor capabilities() const
    {
        return { source_kind::memory, false, false };
    }

    std::optional<resolved_asset> locate(std::string_view package,
                                         std::string_view relative) const
    {
        auto found = m_entries.find(key{ std::string(package), std::string(relative) });
        if(found == m_entries.end())
            return std::nullopt;
        return resolved_asset{ byte_reader{
            std::make_unique<detail::buffer_puller>(found->second) } };
    }

private:
    using key = std::pair<std::string, std::string>;

    std::map<key, std::string> m_entries;
};

}

#endif
