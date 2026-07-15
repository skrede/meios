#ifndef HPP_GUARD_MEIOS_XACRO_STRUCTURAL_DETAIL_H
#define HPP_GUARD_MEIOS_XACRO_STRUCTURAL_DETAIL_H

#include "meios/xacro/budget.h"
#include "meios/xacro/eval_scope.h"
#include "meios/xacro/structural.h"
#include "meios/xacro/eval_policy.h"

#include "meios/diagnostic/log_sink.h"

#include <pugixml.hpp>

#include <map>
#include <memory>
#include <string>
#include <vector>
#include <cstddef>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios
{
class source_stack;
class evaluator_handle;
}

namespace meios::detail
{

struct macro_def
{
    std::vector<std::string> params;
    std::vector<std::optional<std::string>> defaults;
    std::vector<std::string> block_params;
    std::vector<bool> block_children;
    pugi::xml_node body;
};

struct block_arg
{
    bool children;
    pugi::xml_node source;
};

// Threads the whole expansion: name scope, package sources, both budget counters,
// the macro table, the include cycle stack, and the active block bindings. Parsed
// include documents are parked in owned so macro bodies stay live across files.
struct expand_ctx
{
    expand_ctx(eval_scope &s, source_stack &src, const expansion_limits &lim, eval_policy policy,
               evaluator_handle *inject, log_sink &lg);

    eval_scope &scope;
    source_stack &sources;
    const expansion_limits &limits;
    log_sink &log;
    eval_policy mode;
    evaluator_handle *backend;
    expansion_counters counters;
    std::map<std::string, macro_def> macros;
    std::map<std::string, block_arg> blocks;
    std::vector<std::filesystem::path> include_stack;
    std::vector<std::unique_ptr<pugi::xml_document>> owned;
    bool ok;

    bool charge_work();
    bool charge_output();
    pugi::xml_document &park();
};

bool fail(expand_ctx &ctx, const std::string &message);

std::string substitute_attr(expand_ctx &ctx, std::string_view raw,
                            const std::filesystem::path &document, bool &ok);

bool process_children(expand_ctx &ctx, pugi::xml_node in, pugi::xml_node out,
                      const std::filesystem::path &document);
bool process_node(expand_ctx &ctx, pugi::xml_node in, pugi::xml_node out,
                  const std::filesystem::path &document);
bool emit_element(expand_ctx &ctx, pugi::xml_node in, pugi::xml_node out,
                  const std::filesystem::path &document);
bool insert_block(expand_ctx &ctx, pugi::xml_node in, pugi::xml_node out,
                  const std::filesystem::path &document);

bool expand_include(expand_ctx &ctx, pugi::xml_node in, pugi::xml_node out,
                    const std::filesystem::path &document);

void define_macro(expand_ctx &ctx, pugi::xml_node in);
bool instantiate_macro(expand_ctx &ctx, const macro_def &def, pugi::xml_node call,
                       pugi::xml_node out, const std::filesystem::path &document);

}

#endif
