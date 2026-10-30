#include "lexer.h"
#include "eval_parser.h"
#include "reference_names.h"

#include "meios/xacro/value.h"
#include "meios/xacro/eval_scope.h"
#include "meios/xacro/scope_namespace.h"

#include "meios/xacro/detail/scope_read.h"

#include "meios/diagnostic/diagnostic_code.h"

#include <string>
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

// A namespace inside another is read through in turn, and one an expression stops at has no value,
// as a bare one has none.
value inner_member(parser &p, const scope_namespace &outer, const std::string &spelling, std::string_view name)
{
    const std::optional<scope_namespace> inner = p.scope.namespace_of(outer, name);
    if(!inner || !p.accept(token_kind::dot))
        return refuse_namespace_read(p, spelling);
    if(!p.at(token_kind::name))
        return p.fail_unsupported("dotted access in an expression — use eval-python");
    const std::string_view member = p.peek().text;
    ++p.pos;
    return namespace_member(p, *inner, spelling, member);
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
// include was read in and evaluates a deferred member in the table that holds it.
value namespace_member(parser &p, const scope_namespace &space, std::string_view spelling, std::string_view member)
{
    const name_read read = read_member(p.scope, space, member, p.nesting);
    if(read.failed())
        return p.fail_reported(read.failure);
    if(read.space)
        return inner_member(p, space, std::string(spelling) + '.' + std::string(member), member);
    if(!read.bound)
        return p.fail(undefined_name(p.scope, member), diagnostic_code::undefined_property);
    return *read.bound;
}

}
