#include "structural_detail.h"

#include "meios/xacro/value.h"
#include "meios/xacro/structural.h"
#include "meios/xacro/detail/numeric.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"

#include <pugixml.hpp>

#include <string>
#include <memory>
#include <sstream>
#include <cstddef>
#include <filesystem>
#include <string_view>

namespace meios
{

namespace detail
{

expand_ctx::expand_ctx(eval_scope &s, source_stack &src, const expansion_limits &lim, log_sink &lg)
    : scope(s), sources(src), limits(lim), log(lg), counters(), macros(), blocks(),
      include_stack(), owned(), ok(true)
{
}

bool expand_ctx::charge_work()
{
    if(!ok)
        return false;
    if(++counters.work > limits.work)
    {
        log.log(level::error,
                "expansion budget exceeded: work limit of " + std::to_string(limits.work)
                    + " units reached");
        ok = false;
        return false;
    }
    return true;
}

bool expand_ctx::charge_output()
{
    if(!ok)
        return false;
    if(++counters.output_nodes > limits.output_nodes)
    {
        log.log(level::error,
                "expansion budget exceeded: output-node limit of "
                    + std::to_string(limits.output_nodes) + " nodes reached");
        ok = false;
        return false;
    }
    return true;
}

pugi::xml_document &expand_ctx::park()
{
    owned.push_back(std::make_unique<pugi::xml_document>());
    return *owned.back();
}

bool fail(expand_ctx &ctx, const std::string &message)
{
    ctx.log.log(level::error, message);
    ctx.ok = false;
    return false;
}

binding classify(std::string_view text)
{
    bool ok = false;
    long long integer = parse_int(text, ok);
    if(ok)
        return binding{ value{ integer } };
    double real = parse_double(text, ok);
    if(ok)
        return binding{ value{ real } };
    return binding{ std::string(text) };
}

}

// pugixml's default parse flags never load a DTD or resolve external entities, so
// an XXE / entity-expansion payload has no effect; keep it at parse_default.
expansion expand(std::string_view source, eval_scope &scope, source_stack &sources,
                 const std::filesystem::path &document, const expansion_limits &limits,
                 log_sink &log)
{
    detail::expand_ctx ctx(scope, sources, limits, log);
    pugi::xml_document &doc = ctx.park();
    pugi::xml_parse_result parsed = doc.load_buffer(source.data(), source.size());
    if(!parsed)
    {
        log.log(level::error, std::string("xacro parse error: ") + parsed.description());
        return expansion{ false, {} };
    }
    ctx.include_stack.push_back(std::filesystem::weakly_canonical(document));
    pugi::xml_document result;
    for(pugi::xml_node child : doc.children())
        if(!detail::process_node(ctx, child, result, document))
            return expansion{ false, {} };
    std::ostringstream out;
    result.save(out, "", pugi::format_raw);
    return expansion{ ctx.ok, out.str() };
}

}
