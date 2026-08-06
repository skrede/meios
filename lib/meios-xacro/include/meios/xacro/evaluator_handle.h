#ifndef HPP_GUARD_MEIOS_XACRO_EVALUATOR_HANDLE_H
#define HPP_GUARD_MEIOS_XACRO_EVALUATOR_HANDLE_H

#include "meios/xacro/eval_scope.h"
#include "meios/xacro/core_evaluator.h"

#include "meios/diagnostic/log_sink.h"

#include <memory>
#include <string>
#include <cassert>
#include <utility>
#include <concepts>
#include <optional>
#include <string_view>

namespace meios
{

// The value variant cannot carry strings, so an alternate backend is injected at
// the string level: it renders an expression to text and reports why it declined.
// The sink handed to eval_to_text anchors a diagnostic emitted through the message-only
// log(level, message) overload at the node being expanded, and additionally types an error
// as expression_error, since a terminal failure has to carry a code to be returned as the
// structured cause. A warning is anchored and left uncoded. A backend wanting either to
// carry a particular code must emit it through an overload that supplies one.
template <typename E>
concept text_evaluator = requires(E &backend, std::string_view expr,
                                  const eval_scope &scope, log_sink &log)
{
    { backend.eval_to_text(expr, scope, log) } -> std::convertible_to<std::optional<std::string>>;
    { backend.last_failure_kind() } -> std::convertible_to<eval_failure_kind>;
};

// Move-only value type erasing any text_evaluator behind a manual vtable, mirroring
// scanner_handle. A moved-from handle is empty; the dispatch members assert valid()
// so the contract violation is diagnosable under debug.
class evaluator_handle
{
public:
    template <text_evaluator E>
    explicit evaluator_handle(E backend) : m_self(std::make_unique<model<E>>(std::move(backend))) {}

    evaluator_handle(evaluator_handle &&) noexcept = default;
    evaluator_handle &operator=(evaluator_handle &&) noexcept = default;
    evaluator_handle(const evaluator_handle &) = delete;
    evaluator_handle &operator=(const evaluator_handle &) = delete;

    ~evaluator_handle() = default;

    bool valid() const noexcept { return m_self != nullptr; }

    std::optional<std::string> eval_to_text(std::string_view expr, const eval_scope &scope, log_sink &log)
    {
        assert(valid() && "eval_to_text() on a moved-from evaluator_handle");
        return m_self->do_eval(expr, scope, log);
    }

    eval_failure_kind last_failure_kind() const
    {
        assert(valid() && "last_failure_kind() on a moved-from evaluator_handle");
        return m_self->do_kind();
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

        virtual std::optional<std::string> do_eval(std::string_view, const eval_scope &, log_sink &) = 0;
        virtual eval_failure_kind do_kind() const = 0;
    };

    template <text_evaluator E>
    struct model final : concept_t
    {
        explicit model(E backend) : m_backend(std::move(backend)) {}

        std::optional<std::string> do_eval(std::string_view expr, const eval_scope &scope,
                                           log_sink &log) override
        {
            return m_backend.eval_to_text(expr, scope, log);
        }

        eval_failure_kind do_kind() const override { return m_backend.last_failure_kind(); }

        E m_backend;
    };

    std::unique_ptr<concept_t> m_self;
};

}

#endif
