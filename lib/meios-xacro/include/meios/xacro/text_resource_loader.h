#ifndef HPP_GUARD_MEIOS_XACRO_TEXT_RESOURCE_LOADER_H
#define HPP_GUARD_MEIOS_XACRO_TEXT_RESOURCE_LOADER_H

#include "meios/expected.h"

#include <memory>
#include <string>
#include <utility>
#include <filesystem>
#include <string_view>

namespace meios
{

// A fetcher returns a crossed read ceiling without logging it, because only the evaluator holds the
// call site and the policy-routed sink that make it one located, terminal record; it reports every
// other failure itself, since it alone knows the cause. A containment refusal is kept apart from the
// other unreachable documents so that an evaluator reporting its own rules names only that one.
enum class text_resource_failure
{
    uncontained,
    unreachable,
    exhausted
};

using text_resource_result = expected<std::string, text_resource_failure>;

// Move-only type-erased hook handing an evaluator an auxiliary text resource, so the
// evaluator never resolves or opens a path itself. std::move_only_function is C++23 and a
// copyable callable cannot own a move-only implementor, so this is the same
// unique_ptr-behind-a-vtable shape the bundle rewrite hook and the asset byte stream use.
// The document is a per-call argument, not a construction capture: a relative spec belongs
// to the file it is written in, and that file changes as an expansion descends an include.
class text_resource_loader
{
public:
    struct fetcher
    {
        fetcher()                           = default;
        fetcher(const fetcher &)            = default;
        fetcher &operator=(const fetcher &) = default;
        fetcher(fetcher &&)                 = default;
        fetcher &operator=(fetcher &&)      = default;
        virtual ~fetcher()                  = default;

        virtual text_resource_result fetch(std::string_view spec, const std::filesystem::path &document) = 0;
    };

    text_resource_loader() = default;
    explicit text_resource_loader(std::unique_ptr<fetcher> impl)
            : m_impl(std::move(impl))
    {
    }

    text_resource_loader(text_resource_loader &&) noexcept            = default;
    text_resource_loader &operator=(text_resource_loader &&) noexcept = default;
    text_resource_loader(const text_resource_loader &)                = delete;
    text_resource_loader &operator=(const text_resource_loader &)     = delete;

    ~text_resource_loader() = default;

    bool valid() const noexcept
    {
        return m_impl != nullptr;
    }

    text_resource_result operator()(std::string_view spec, const std::filesystem::path &document) const
    {
        return m_impl->fetch(spec, document);
    }

private:
    std::unique_ptr<fetcher> m_impl;
};

}

#endif
