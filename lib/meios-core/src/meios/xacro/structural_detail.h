#ifndef HPP_GUARD_MEIOS_XACRO_STRUCTURAL_DETAIL_H
#define HPP_GUARD_MEIOS_XACRO_STRUCTURAL_DETAIL_H

#include "expand_context.h"

#include <pugixml.hpp>

#include <string>
#include <cstddef>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios::detail
{

void record_scoped(expand_ctx &ctx, std::string_view scope_attr, std::string_view name);

void seed_declared_args(eval_scope &scope, pugi::xml_node node);

std::string substitute_attr(expand_ctx &ctx, pugi::xml_node in, std::string_view raw,
                            const std::filesystem::path &document, bool &ok,
                            std::optional<std::size_t> attr_index = std::nullopt);

value substitute_attr_value(expand_ctx &ctx, pugi::xml_node in, std::string_view raw,
                            const std::filesystem::path &document, bool &ok,
                            std::optional<std::size_t> attr_index = std::nullopt);

value substitute_arg_value(expand_ctx &ctx, pugi::xml_node in, std::string_view raw,
                           const std::filesystem::path &document, bool &ok,
                           std::optional<std::size_t> attr_index = std::nullopt);

std::string strip_container_marker(std::string text);

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

bool define_property(expand_ctx &ctx, pugi::xml_node in, const std::filesystem::path &document);
bool declare_arg(expand_ctx &ctx, pugi::xml_node in, const std::filesystem::path &document);

void parse_params(std::string_view spec, macro_def &def);

void define_macro(expand_ctx &ctx, pugi::xml_node in);
bool instantiate_macro(expand_ctx &ctx, const macro_def &def, pugi::xml_node call,
                       pugi::xml_node out, const std::filesystem::path &document);

}

#endif
