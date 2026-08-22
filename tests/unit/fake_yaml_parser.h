#ifndef HPP_GUARD_MEIOS_UNIT_FAKE_YAML_PARSER_H
#define HPP_GUARD_MEIOS_UNIT_FAKE_YAML_PARSER_H

#include <meios/xacro.h>

#include <memory>
#include <string>
#include <vector>
#include <cstddef>
#include <utility>
#include <optional>
#include <algorithm>
#include <string_view>

namespace fake
{

struct yaml_line
{
    bool item;
    std::size_t indent;
    std::string key;
    std::string text;
};

inline std::optional<yaml_line> yaml_pair(std::string_view line, std::size_t start)
{
    const std::size_t colon = line.find(':');
    if(colon == std::string_view::npos)
        return std::nullopt;
    const std::string_view rest = line.substr(colon + 1);
    const std::size_t value_at = rest.find_first_not_of(' ');
    return yaml_line{ false, start, std::string(line.substr(start, colon - start)),
                      value_at == std::string_view::npos
                          ? std::string()
                          : std::string(rest.substr(value_at)) };
}

inline std::optional<yaml_line> yaml_row(std::string_view line)
{
    // YAML 1.2 (§5.4) counts CRLF as a single line break. Left in place, the carriage
    // return is trailing value text, so a key introducing a nested block reads as a
    // scalar and the whole nesting below it is lost.
    if(line.ends_with('\r'))
        line.remove_suffix(1);
    const std::size_t start = line.find_first_not_of(' ');
    if(start == std::string_view::npos)
        return std::nullopt;
    if(line.substr(start).starts_with("- "))
        return yaml_line{ true, start, std::string(), std::string(line.substr(start + 2)) };
    return yaml_pair(line, start);
}

inline std::vector<yaml_line> yaml_lines(std::string_view bytes)
{
    std::vector<yaml_line> rows;
    for(std::size_t at = 0; at < bytes.size();)
    {
        const std::size_t stop = std::min(bytes.find('\n', at), bytes.size());
        const std::optional<yaml_line> row = yaml_row(bytes.substr(at, stop - at));
        at = stop + 1;
        if(row)
            rows.push_back(*row);
    }
    return rows;
}

// Marked at every depth, because the module's own reader marks at its single delivery seam and
// a stand-in that left the tree unmarked would refuse a dotted read the reader admits.
inline meios::value yaml_scalar(const std::string &text)
{
    if(text == "true" || text == "false")
        return meios::value{ text == "true" }.with_yaml_origin();
    return meios::detail::classify(text).with_yaml_origin();
}

inline meios::value yaml_mapping(const std::vector<yaml_line> &rows, std::size_t &at,
                                 std::size_t indent);

inline meios::value yaml_sequence(const std::vector<yaml_line> &rows, std::size_t &at,
                                  std::size_t indent)
{
    std::vector<meios::value> items;
    while(at < rows.size() && rows[at].item && rows[at].indent == indent)
        items.push_back(yaml_scalar(rows[at++].text));
    return meios::value::make_sequence(std::move(items)).with_yaml_origin();
}

inline meios::value yaml_block(const std::vector<yaml_line> &rows, std::size_t &at,
                               std::size_t indent)
{
    if(rows[at].item)
        return yaml_sequence(rows, at, indent);
    return yaml_mapping(rows, at, indent);
}

inline meios::value yaml_mapping(const std::vector<yaml_line> &rows, std::size_t &at,
                                 std::size_t indent)
{
    std::vector<meios::value::entry> entries;
    while(at < rows.size() && !rows[at].item && rows[at].indent == indent)
    {
        const yaml_line row = rows[at++];
        if(!row.text.empty())
            entries.emplace_back(row.key, yaml_scalar(row.text));
        else if(at < rows.size() && rows[at].indent > indent)
            entries.emplace_back(row.key, yaml_block(rows, at, rows[at].indent));
        else
            entries.emplace_back(row.key, meios::value{}.with_yaml_origin());
    }
    return meios::value::make_mapping(std::move(entries)).with_yaml_origin();
}

// A deliberately small block reader: mappings and sequences nested by indentation, one plain
// scalar per leaf, and no other YAML construct. It exists so the expression grammar is provable
// with no third-party dependency and no optional module built — it is a fake, not a parser.
class yaml_parser final : public meios::yaml_parser_handle::parser
{
public:
    meios::yaml_outcome parse(std::string_view bytes, const meios::evaluator_limits &,
                              meios::evaluator_counters &, meios::log_sink &,
                              const meios::source_location &) const override
    {
        const std::vector<yaml_line> rows = yaml_lines(bytes);
        std::size_t at = 0;
        if(rows.empty())
            return meios::yaml_outcome{ meios::value::make_mapping({}).with_yaml_origin(),
                                        meios::yaml_failure::none };
        return meios::yaml_outcome{ yaml_block(rows, at, rows.front().indent),
                                    meios::yaml_failure::none };
    }
};

inline std::shared_ptr<const meios::yaml_parser_handle> yaml_parser_handle()
{
    return std::make_shared<const meios::yaml_parser_handle>(std::make_unique<yaml_parser>());
}

}

#endif
