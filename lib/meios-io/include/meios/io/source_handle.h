#ifndef HPP_GUARD_MEIOS_IO_SOURCE_HANDLE_H
#define HPP_GUARD_MEIOS_IO_SOURCE_HANDLE_H

#include "meios/io/package_source.h"
#include "meios/io/source_lookup.h"
#include "meios/io/resolved_asset.h"

#include <string>
#include <memory>
#include <vector>
#include <cassert>
#include <utility>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios
{

// Move-only value type that erases any package_source behind a manual vtable.
// The optional capabilities (path_of, packages) are resolved at this boundary
// via if constexpr, so a bytes-only source needs no virtual to opt out.
class source_handle
{
public:
    template<package_source S>
    explicit source_handle(S s)
            : m_self(std::make_unique<model<S>>(std::move(s)))
    {
    }

    source_handle(source_handle &&) noexcept            = default;
    source_handle &operator=(source_handle &&) noexcept = default;
    source_handle(const source_handle &)                = delete;
    source_handle &operator=(const source_handle &)     = delete;

    ~source_handle() = default;

    // A moved-from handle is empty; every dispatch member requires a live handle
    // and asserts valid() so the contract violation is diagnosable under debug.
    bool valid() const noexcept
    {
        return m_self != nullptr;
    }

    capability_descriptor capabilities() const
    {
        assert(valid() && "capabilities() on a moved-from source_handle");
        return m_self->do_caps();
    }

    std::optional<resolved_asset> locate(std::string_view pkg, std::string_view rel)
    {
        assert(valid() && "locate() on a moved-from source_handle");
        return m_self->do_locate(pkg, rel);
    }

    source_lookup_result try_locate(std::string_view pkg, std::string_view rel)
    {
        assert(valid() && "try_locate() on a moved-from source_handle");
        return m_self->do_try_locate(pkg, rel);
    }

    std::optional<std::filesystem::path> path_of(std::string_view pkg, std::string_view rel) const
    {
        assert(valid() && "path_of() on a moved-from source_handle");
        return m_self->do_path(pkg, rel);
    }

    std::vector<std::string> packages() const
    {
        assert(valid() && "packages() on a moved-from source_handle");
        return m_self->do_enumerate();
    }

private:
    struct concept_t
    {
        concept_t()                             = default;
        concept_t(const concept_t &)            = default;
        concept_t &operator=(const concept_t &) = default;
        concept_t(concept_t &&)                 = default;
        concept_t &operator=(concept_t &&)      = default;
        virtual ~concept_t()                    = default;

        virtual capability_descriptor do_caps() const                                                  = 0;
        virtual std::optional<resolved_asset> do_locate(std::string_view, std::string_view)            = 0;
        virtual source_lookup_result do_try_locate(std::string_view, std::string_view)                 = 0;
        virtual std::optional<std::filesystem::path> do_path(std::string_view, std::string_view) const = 0;
        virtual std::vector<std::string> do_enumerate() const                                          = 0;
    };

    template<package_source S>
    struct model final : concept_t
    {
        explicit model(S s)
                : m_src(std::move(s))
        {
        }

        capability_descriptor do_caps() const override
        {
            return m_src.capabilities();
        }

        std::optional<resolved_asset> do_locate(std::string_view pkg, std::string_view rel) override
        {
            return m_src.locate(pkg, rel);
        }

        source_lookup_result do_try_locate(std::string_view pkg, std::string_view rel) override
        {
            if constexpr(provides_typed_lookup<S>)
                return m_src.try_locate(pkg, rel);
            else
                return m_src.locate(pkg, rel);
        }

        std::optional<std::filesystem::path> do_path(std::string_view pkg, std::string_view rel) const override
        {
            if constexpr(provides_path<S>)
            {
                return m_src.path_of(pkg, rel);
            }
            else
            {
                (void)pkg;
                (void)rel;
                return std::nullopt;
            }
        }

        std::vector<std::string> do_enumerate() const override
        {
            if constexpr(enumerates_packages<S>)
                return m_src.packages();
            else
                return {};
        }

        S m_src;
    };

    std::unique_ptr<concept_t> m_self;
};

}

#endif
