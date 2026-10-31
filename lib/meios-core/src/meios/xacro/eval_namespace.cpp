#include "lexer.h"
#include "eval_parser.h"
#include "reference_names.h"

#include "meios/xacro/value.h"
#include "meios/xacro/eval_scope.h"
#include "meios/xacro/scope_namespace.h"

#include "meios/xacro/detail/scope_read.h"

#include "meios/diagnostic/diagnostic_code.h"

#include <string>
#include <cstddef>
#include <optional>
#include <string_view>

namespace meios::detail
{
namespace
{

// Upstream's math and python namespaces exist under neither evaluator, which bind those names bare.
std::string unbound_head(std::string_view head)
{
    if(head == "math")
        return " — neither evaluator binds a math namespace; write the bare name, as pi or sin(x)";
    if(head == "python")
        return " — neither evaluator binds a python namespace; the python evaluator binds its builtins bare, as len(x)";
    if(listed(mathematics_names, head) || listed(python_builtins, head))
        return " — use eval-python";
    return " — neither evaluator binds '" + std::string(head) + "'";
}

// Each hop of a dotted read holds one level of the shared expression-depth ceiling until the walk
// returns, so no chain of namespaces is longer than the ceiling every other nesting shares.
class hop_levels
{
public:
    explicit hop_levels(parser &owner)
            : m_p(owner)
            , m_held(0)
    {
    }

    hop_levels(const hop_levels &)            = delete;
    hop_levels &operator=(const hop_levels &) = delete;

    ~hop_levels()
    {
        for(; m_held > 0; --m_held)
            m_p.leave_level();
    }

    bool enter()
    {
        if(!m_p.enter_level())
            return false;
        ++m_held;
        return true;
    }

private:
    parser &m_p;
    std::size_t m_held;
};

value member_value(parser &p, const name_read &read, std::string_view name)
{
    if(read.failed())
        return p.fail_reported(read.failure);
    if(!read.bound)
        return p.fail(undefined_name(p.scope, name), diagnostic_code::undefined_property);
    return *read.bound;
}

// The members after the first sit at every second token from the walk's start, each behind its dot.
std::string walked_spelling(const parser &p, std::string_view head, std::string_view member, std::size_t from)
{
    std::string spelling = std::string(head) + '.' + std::string(member);
    for(std::size_t at = from + 1; at < p.pos; at += 2)
        spelling += '.' + std::string(p.tokens[at].text);
    return spelling;
}

}

// Both python evaluators bind the xacro namespace to load_yaml alone, so advising one for any
// other helper would send the reader to an evaluator that refuses the same call.
value refuse_call_target(parser &p, std::string_view head, std::string_view member)
{
    const std::string target = "unsupported call target '" + std::string(head) + '.' + std::string(member) + "'";
    if(head != "xacro")
        return p.fail_unsupported(target + unbound_head(head));
    std::string message = target + " — xacro.load_yaml is the only helper either evaluator exposes";
    if(member == "arg")
        message += "; an argument is read with the $(arg) command";
    return p.fail_unsupported(message);
}

// The reference's namespace forwards a dot to its own lookup, which falls through to the table the
// include was read in and evaluates a deferred member in the table that holds it. A namespace inside
// another is read through in turn, and one an expression stops at has no value, as a bare one has none.
value namespace_member(parser &p, const scope_namespace &space, std::string_view spelling, std::string_view member)
{
    hop_levels levels(p);
    const std::size_t from            = p.pos;
    std::optional<scope_namespace> at = space;
    for(std::string_view name = member;;)
    {
        if(!levels.enter())
            return value{};
        const name_read read = read_member(p.scope, *at, name, p.nesting);
        if(read.failed() || !read.space)
            return member_value(p, read, name);
        at = p.scope.namespace_of(*at, name);
        if(!at || !p.accept(token_kind::dot))
            return refuse_namespace_read(p, walked_spelling(p, spelling, member, from));
        if(!p.at(token_kind::name))
            return p.fail_unsupported("dotted access in an expression — use eval-python");
        name = p.peek().text;
        ++p.pos;
    }
}

}
