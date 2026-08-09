#ifndef HPP_GUARD_MEIOS_XACRO_EVALUATOR_HANDLE_H
#define HPP_GUARD_MEIOS_XACRO_EVALUATOR_HANDLE_H

#include "meios/xacro/eval_scope.h"
#include "meios/xacro/core_evaluator.h"

#include "meios/diagnostic/log_sink.h"

#include <memory>
#include <cassert>
#include <utility>
#include <concepts>
#include <string_view>

namespace meios
{

// An injected backend renders an expression to text while the built-in path answers with a
// value, so the seam is spelled at the string level; the failure kind comes back inside the
// outcome rather than through an accessor, which is what lets one handle be shared across
// concurrent loads without either load reading the other's state.
// The sink handed to eval_to_text anchors a diagnostic emitted through the message-only
// log(level, message) overload at the node being expanded, and additionally types an error
// as expression_error, since a terminal failure has to carry a code to be returned as the
// structured cause. A warning is anchored and left uncoded. A backend wanting either to
// carry a particular code must emit it through an overload that supplies one.
template <typename E>
concept text_evaluator = requires(E &backend, std::string_view expr,
                                  const eval_scope &scope, log_sink &log)
{
    { backend.eval_to_text(expr, scope, log) } -> std::convertible_to<text_outcome>;
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

    text_outcome eval_to_text(std::string_view expr, const eval_scope &scope, log_sink &log)
    {
        assert(valid() && "eval_to_text() on a moved-from evaluator_handle");
        return m_self->do_eval(expr, scope, log);
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

        virtual text_outcome do_eval(std::string_view, const eval_scope &, log_sink &) = 0;
    };

    template <text_evaluator E>
    struct model final : concept_t
    {
        explicit model(E backend) : m_backend(std::move(backend)) {}

        text_outcome do_eval(std::string_view expr, const eval_scope &scope, log_sink &log) override
        {
            return m_backend.eval_to_text(expr, scope, log);
        }

        E m_backend;
    };

    std::unique_ptr<concept_t> m_self;
};

}

#endif
