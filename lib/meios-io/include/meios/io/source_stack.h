#ifndef HPP_GUARD_MEIOS_IO_SOURCE_STACK_H
#define HPP_GUARD_MEIOS_IO_SOURCE_STACK_H

#include "meios/io/source_handle.h"
#include "meios/io/source_lookup.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"

#include <span>
#include <string>
#include <vector>
#include <cstddef>
#include <utility>
#include <optional>
#include <string_view>

namespace meios
{

// Variadic front door that erases a pack of sources into an ordered vector.
// Index 0 is the highest precedence; a lower-precedence layer that could also answer a query is
// surfaced as an info shadow diagnostic, never silently hidden. A layer that models answers is asked
// through it alone, so its own refusal cannot fail a lookup a higher one answered; a layer that does
// not is asked through locate, with whatever that logs or writes.
class source_stack
{
public:
    template<package_source... Ss>
    explicit source_stack(Ss... sources)
            : m_layers()
    {
        m_layers.reserve(sizeof...(Ss));
        (m_layers.emplace_back(source_handle(std::move(sources))), ...);
    }

    source_stack &push_back(source_handle handle)
    {
        m_layers.push_back(std::move(handle));
        return *this;
    }

    std::span<source_handle> layers() noexcept
    {
        return m_layers;
    }

    std::span<const source_handle> layers() const noexcept
    {
        return m_layers;
    }

    std::size_t size() const noexcept
    {
        return m_layers.size();
    }

    bool empty() const noexcept
    {
        return m_layers.empty();
    }

    std::optional<resolved_asset> locate(std::string_view pkg, std::string_view rel, log_sink &log)
    {
        std::optional<resolved_asset> hit;
        std::size_t rank = 0;
        for(; rank < m_layers.size() && !hit; ++rank)
            hit = m_layers[rank].locate(pkg, rel);
        note_shadows(pkg, rel, rank, log);
        return hit;
    }

    source_lookup_result try_locate(std::string_view pkg, std::string_view rel, log_sink &log)
    {
        std::optional<resolved_asset> hit;
        std::optional<operation_failure> first_failure;
        std::size_t rank = 0;
        for(; rank < m_layers.size() && !hit; ++rank)
            consider(m_layers[rank].try_locate(pkg, rel), hit, first_failure);
        note_shadows(pkg, rel, rank, log);
        if(hit)
            return hit;
        if(first_failure)
            return unexpected<operation_failure>(*first_failure);
        return std::optional<resolved_asset>{};
    }

private:
    std::vector<source_handle> m_layers;

    static void consider(source_lookup_result found, std::optional<resolved_asset> &hit, std::optional<operation_failure> &failure)
    {
        if(!found)
        {
            if(!failure)
                failure = found.error();
            return;
        }
        if(!*found)
            return;
        hit = std::move(*found);
        failure.reset();
    }

    void note_shadows(std::string_view pkg, std::string_view rel, std::size_t from, log_sink &log)
    {
        for(std::size_t rank = from; rank < m_layers.size(); ++rank)
            if(m_layers[rank].answers(pkg, rel))
                log.log(level::info, shadow_message(pkg, rel, rank));
    }

    static std::string shadow_message(std::string_view pkg, std::string_view rel, std::size_t rank)
    {
        return "package path \"" + std::string(pkg) + '/' + std::string(rel) + "\" also resolved by lower-precedence layer " + std::to_string(rank) +
                "; shadowed by a higher-precedence source";
    }
};

}

#endif
