#ifndef HPP_GUARD_MEIOS_BUNDLE_SCANNER_HANDLE_H
#define HPP_GUARD_MEIOS_BUNDLE_SCANNER_HANDLE_H

#include "meios/bundle/asset_scanner.h"

#include "meios/io/resolved_asset.h"

#include "meios/diagnostic/log_sink.h"

#include <memory>
#include <string>
#include <vector>
#include <cassert>
#include <utility>

namespace meios
{

// Move-only value type that erases any asset_scanner behind a manual vtable. A
// moved-from handle is empty; the dispatch member asserts valid() so the contract
// violation is diagnosable under debug.
class scanner_handle
{
public:
    template <asset_scanner S>
    explicit scanner_handle(S s) : m_self(std::make_unique<model<S>>(std::move(s))) {}

    scanner_handle(scanner_handle &&) noexcept = default;
    scanner_handle &operator=(scanner_handle &&) noexcept = default;
    scanner_handle(const scanner_handle &) = delete;
    scanner_handle &operator=(const scanner_handle &) = delete;

    ~scanner_handle() = default;

    bool valid() const noexcept { return m_self != nullptr; }

    std::vector<std::string> scan(const resolved_asset &asset, log_sink &log)
    {
        assert(valid() && "scan() on a moved-from scanner_handle");
        return m_self->do_scan(asset, log);
    }

private:
    struct concept_t
    {
        concept_t() = default;
        concept_t(const concept_t &) = default;
        concept_t &operator=(const concept_t &) = default;
        concept_t(concept_t &&) = default;
        concept_t &operator=(concept_t &&) = default;
        virtual ~concept_t() = default;

        virtual std::vector<std::string> do_scan(const resolved_asset &, log_sink &) = 0;
    };

    template <asset_scanner S>
    struct model final : concept_t
    {
        explicit model(S s) : m_src(std::move(s)) {}

        std::vector<std::string> do_scan(const resolved_asset &asset, log_sink &log) override
        {
            return m_src.scan(asset, log);
        }

        S m_src;
    };

    std::unique_ptr<concept_t> m_self;
};

}

#endif
