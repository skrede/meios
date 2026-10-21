#include "generated_record.h"

#include <meios/xacro.h>

#include <meios/io/source_stack.h>
#include <meios/io/directory_source.h>

#include <meios/diagnostic/log_sink.h>

#include <pugixml.hpp>

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <string>
#include <vector>
#include <cstddef>
#include <filesystem>
#include <string_view>

#ifndef MEIOS_GENERATED_FIXTURE_DIR
    #error "MEIOS_GENERATED_FIXTURE_DIR must name the generated fixture tree"
#endif

namespace
{

const std::filesystem::path fixture_tree{MEIOS_GENERATED_FIXTURE_DIR};

void replace_all(std::string &text, const std::string &from, std::string_view to)
{
    for(std::size_t at = text.find(from); !from.empty() && at != std::string::npos; at = text.find(from, at + to.size()))
        text.replace(at, from.size(), to);
}

std::string normalized(std::string name)
{
    for(const std::filesystem::path &dir : {fixture_tree / "R", fixture_tree / "doc"})
    {
        const std::string_view token = dir.filename() == "R" ? "@ROOT@" : "@DOC@";
        replace_all(name, dir.string(), token);
        replace_all(name, dir.generic_string(), token);
    }
    return name;
}

std::string quoted_name(std::string_view name)
{
    std::string out = "\"";
    for(const char c : name)
        out += c == '\\' ? std::string("\\\\") : c == '"' ? std::string("\\\"") : c == '\t' ? std::string("\\t") : c == '\n' ? std::string("\\n") : std::string(1, c);
    return out + '"';
}

void collect_links(const pugi::xml_node &node, std::string &out)
{
    for(const pugi::xml_node &child : node.children())
    {
        if(child.type() != pugi::node_element)
            continue;
        if(std::string_view(child.name()) == "link")
            out += (out.empty() ? "" : " ") + quoted_name(normalized(child.attribute("name").value()));
        collect_links(child, out);
    }
}

std::string link_names(const std::string &document)
{
    pugi::xml_document parsed;
    REQUIRE(parsed.load_string(document.c_str()));
    std::string out;
    collect_links(parsed, out);
    return out.empty() ? std::string("-") : out;
}

std::string meios_verdict(const generated::record &held, const generated::generated_case &one)
{
    meios::log_sink silent;
    meios::source_stack sources;
    sources.push_back(meios::source_handle(meios::directory_source(fixture_tree / "R", silent)));
    meios::eval_scope scope;
    const meios::expected<meios::expansion, meios::expansion_error> out =
            meios::expand(generated::document_of(held, one), scope, sources, fixture_tree / "doc" / "case.xacro", meios::expansion_limits{}, meios::eval_policy::fail, nullptr, silent,
                          meios::include_options(), {fixture_tree / "R"});
    return out ? link_names(out->document) : std::string("REFUSED");
}

const generated::generated_case *case_named(const generated::record &held, const std::string &id)
{
    for(const generated::generated_case &one : held.cases)
        if(one.id == id)
            return &one;
    return nullptr;
}

bool reviewed(const std::vector<oracle::row> &rows, const generated::generated_case &one, const std::string &meios)
{
    for(const oracle::row &row : rows)
        if(row.fields.size() >= 3 && row.fields[0] == one.id)
            return row.fields[1] == one.upstream && row.fields[2] == meios;
    return false;
}

}

TEST_CASE("every generated case agrees with the pinned reference or is a reviewed divergence", "[generated]")
{
    const generated::record held        = generated::read_record();
    const std::vector<oracle::row> rows = generated::reviewed_rows();
    REQUIRE_FALSE(held.cases.empty());
    for(const generated::generated_case &one : held.cases)
    {
        const std::string meios = meios_verdict(held, one);
        if(meios == one.upstream || reviewed(rows, one, meios))
            continue;
        FAIL_CHECK("unreviewed generated divergence\t" << one.id << '\t' << one.upstream << '\t' << meios << "\ttext: " << one.text);
    }
}

TEST_CASE("every reviewed generated divergence still disagrees as recorded", "[generated]")
{
    const generated::record held = generated::read_record();
    for(const oracle::row &row : generated::reviewed_rows())
    {
        REQUIRE(row.fields.size() == 4);
        INFO("reviewed row " << row.fields[0]);
        const generated::generated_case *one = case_named(held, row.fields[0]);
        REQUIRE(one != nullptr);
        CHECK(row.fields[1] == one->upstream);
        CHECK(row.fields[2] == meios_verdict(held, *one));
        CHECK(row.fields[1] != row.fields[2]);
    }
}

TEST_CASE("the record holds the committed count in every family", "[generated]")
{
    const generated::record held = generated::read_record();
    REQUIRE_FALSE(held.families.empty());
    std::map<std::string, std::size_t> counted;
    for(const generated::generated_case &one : held.cases)
        ++counted[one.family];
    for(const auto &[name, layout] : held.families)
    {
        INFO("family " << name);
        CHECK(layout.count > 0);
        CHECK(counted[name] == layout.count);
        CHECK_FALSE(layout.layout.empty());
    }
    CHECK(counted.size() == held.families.size());
}
