#include "structural_detail.h"

#include "meios/xacro/value.h"
#include "meios/xacro/substitution.h"
#include "meios/xacro/core_evaluator.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"

#include <pugixml.hpp>

#include <map>
#include <cctype>
#include <string>
#include <variant>
#include <filesystem>
#include <string_view>

namespace meios::detail
{

namespace
{

bool is_true(const value &v)
{
    if(std::holds_alternative<bool>(v))
        return std::get<bool>(v);
    if(std::holds_alternative<long long>(v))
        return std::get<long long>(v) != 0;
    return std::get<double>(v) != 0.0;
}

bool define_property(expand_ctx &ctx, pugi::xml_node in, const std::filesystem::path &document)
{
    bool ok = true;
    std::string value_text = substitute_attr(ctx, in.attribute("value").value(), document, ok);
    if(!ok)
        return false;
    std::string_view name = in.attribute("name").value();
    record_scoped(ctx, in.attribute("scope").value(), name);
    ctx.scope.set(name, classify(value_text));
    return true;
}

// A declared default seeds the scope only when the name is still unbound, so a
// caller override or an earlier binding wins; the declaration itself emits nothing.
// The default is resolved through substitution first, so a nested
// $(find)/$(arg)/${} default becomes real text rather than a raw literal.
bool declare_arg(expand_ctx &ctx, pugi::xml_node in, const std::filesystem::path &document)
{
    std::string_view name = in.attribute("name").value();
    if(name.empty())
        return fail(ctx, "<xacro:arg> requires a name attribute");
    pugi::xml_attribute fallback = in.attribute("default");
    if(!fallback || ctx.scope.contains(name))
        return true;
    bool ok = true;
    std::string resolved = substitute_attr(ctx, fallback.value(), document, ok);
    if(!ok)
        return false;
    ctx.scope.set(name, classify(resolved));
    return true;
}

std::string lowered(std::string_view text)
{
    std::string out;
    for(char c : text)
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    return out;
}

// xacro accepts the literal booleans true/false/1/0 (case-insensitive) as well as
// a ${} expression; substitution resolves the expression, then the result is
// coerced. An unrecognized non-empty string is evaluated as a fallback expression.
bool condition_true(expand_ctx &ctx, const std::string &text)
{
    std::string flag = lowered(text);
    if(flag == "true" || flag == "1")
        return true;
    if(flag == "false" || flag == "0" || flag.empty())
        return false;
    core_evaluator evaluator;
    value result = evaluator.eval(text, ctx.scope, ctx.log);
    if(evaluator.failed())
        ctx.ok = false;
    return is_true(result);
}

bool conditional(expand_ctx &ctx, pugi::xml_node in, pugi::xml_node out,
                 const std::filesystem::path &document)
{
    // A structural conditional cannot be left half-expanded, so its test is always
    // resolved with fail policy; eval_policy leniency reaches text/attribute spans only.
    substitution result = substitute(in.attribute("value").value(), ctx.scope, ctx.sources,
                                     document, eval_policy::fail, ctx.backend, ctx.log);
    if(!result.ok)
    {
        ctx.ok = false;
        return false;
    }
    bool truth = condition_true(ctx, result.text);
    if(!ctx.ok)
        return false;
    bool wants_true = std::string_view(in.name()) == "xacro:if";
    if(truth == wants_true)
        return process_children(ctx, in, out, document);
    return true;
}

bool dispatch_element(expand_ctx &ctx, pugi::xml_node in, pugi::xml_node out,
                      const std::filesystem::path &document)
{
    std::string_view name = in.name();
    if(name == "xacro:property")
        return define_property(ctx, in, document);
    if(name == "xacro:macro")
        return (define_macro(ctx, in), true);
    if(name == "xacro:arg")
        return declare_arg(ctx, in, document);
    if(name == "xacro:include")
        return expand_include(ctx, in, out, document);
    if(name == "xacro:if" || name == "xacro:unless")
        return conditional(ctx, in, out, document);
    if(name == "xacro:insert_block")
        return insert_block(ctx, in, out, document);
    if(name.rfind("xacro:", 0) != 0)
        return emit_element(ctx, in, out, document);
    std::map<std::string, macro_def>::iterator found = ctx.macros.find(std::string(name.substr(6)));
    if(found != ctx.macros.end())
        return instantiate_macro(ctx, found->second, in, out, document);
    return fail(ctx, "unknown xacro element <" + std::string(name) + '>');
}

}

std::string substitute_attr(expand_ctx &ctx, std::string_view raw,
                            const std::filesystem::path &document, bool &ok)
{
    substitution result = substitute(raw, ctx.scope, ctx.sources, document, ctx.mode, ctx.backend,
                                     ctx.log);
    ok = result.ok;
    if(!ok)
        ctx.ok = false;
    return result.text;
}

bool process_node(expand_ctx &ctx, pugi::xml_node in, pugi::xml_node out,
                  const std::filesystem::path &document)
{
    if(!ctx.charge_work())
        return false;
    pugi::xml_node_type kind = in.type();
    if(kind == pugi::node_pcdata || kind == pugi::node_cdata)
    {
        bool ok = true;
        std::string text = substitute_attr(ctx, in.value(), document, ok);
        if(!ok)
            return false;
        out.append_child(kind).set_value(text.c_str());
        return true;
    }
    if(kind != pugi::node_element)
        return true;
    return dispatch_element(ctx, in, out, document);
}

bool process_children(expand_ctx &ctx, pugi::xml_node in, pugi::xml_node out,
                      const std::filesystem::path &document)
{
    for(pugi::xml_node child : in.children())
        if(!process_node(ctx, child, out, document))
            return false;
    return true;
}

}
