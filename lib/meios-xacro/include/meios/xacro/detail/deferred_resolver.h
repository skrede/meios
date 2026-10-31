#ifndef HPP_GUARD_MEIOS_XACRO_DEFERRED_RESOLVER_H
#define HPP_GUARD_MEIOS_XACRO_DEFERRED_RESOLVER_H

#include "meios/xacro/detail/scope_entry.h"

#include "meios/xacro/eval_failure_kind.h"

#include <string>
#include <vector>
#include <cstddef>
#include <utility>
#include <optional>
#include <functional>
#include <string_view>

namespace meios
{

namespace detail
{

struct resolve_step
{
    std::size_t table;
    std::string name;
    std::size_t origin;
};

using deferred_resolve = std::function<name_read(const deferred_text &, std::size_t)>;
using deferred_cycle = std::function<eval_failure_kind(const std::vector<resolve_step> &, std::string_view)>;

// A name is being resolved per table, so reading a name of the same spelling in another table while
// the first resolves is no cycle.
class deferred_resolver
{
public:
    deferred_resolver()
            : m_cycle_cb()
            , m_resolve_cb()
            , m_resolving()
    {
    }

    void attach(deferred_resolve resolve, deferred_cycle cycle)
    {
        m_resolve_cb = std::move(resolve);
        m_cycle_cb = std::move(cycle);
    }

    void detach()
    {
        m_resolve_cb = nullptr;
        m_cycle_cb = nullptr;
        m_resolving.clear();
    }

    bool attached() const
    {
        return static_cast<bool>(m_resolve_cb);
    }

    std::optional<eval_failure_kind> cycle(std::size_t table, std::string_view name) const
    {
        for(const resolve_step &step : m_resolving)
            if(step.table == table && step.name == name)
                return m_cycle_cb(m_resolving, name);
        return std::nullopt;
    }

    name_read evaluate(std::size_t table, std::string_view name, const deferred_text &deferred, std::size_t carried)
    {
        m_resolving.push_back(resolve_step{ table, std::string(name), deferred.origin });
        name_read answer = m_resolve_cb(deferred, carried);
        m_resolving.pop_back();
        return answer;
    }

private:
    deferred_cycle m_cycle_cb;
    deferred_resolve m_resolve_cb;
    std::vector<resolve_step> m_resolving;
};

}

}

#endif
