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
    std::size_t indent;
    std::string key;
    std::string text;
};

inline std::optional<yaml_line> yaml_row(std::string_view line)
{
    // YAML 1.2 (§5.4) counts CRLF as a single line break. Left in place, the carriage
    // return is trailing value text, so a key introducing a nested block reads as a
    // scalar and the whole nesting below it is lost.
    if(line.ends_with('\r'))
        line.remove_suffix(1);
    const std::size_t start = line.find_first_not_of(' ');
    const std::size_t colon = line.find(':');
    if(start == std::string_view::npos || colon == std::string_view::npos)
        return std::nullopt;
    const std::string_view rest = line.substr(colon + 1);
    const std::size_t value_at = rest.find_first_not_of(' ');
    return yaml_line{ start, std::string(line.substr(start, colon - start)),
                      value_at == std::string_view::npos
                          ? std::string()
                          : std::string(rest.substr(value_at)) };
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

inline meios::value yaml_scalar(const std::string &text)
{
    if(text == "true" || text == "false")
        return meios::value{ text == "true" };
    return meios::detail::classify(text);
}

inline meios::value yaml_mapping(const std::vector<yaml_line> &rows, std::size_t &at,
                                 std::size_t indent)
{
    std::vector<meios::value::entry> entries;
    while(at < rows.size() && rows[at].indent == indent)
    {
        const yaml_line row = rows[at++];
        if(!row.text.empty())
            entries.emplace_back(row.key, yaml_scalar(row.text));
        else if(at < rows.size() && rows[at].indent > indent)
            entries.emplace_back(row.key, yaml_mapping(rows, at, rows[at].indent));
        else
            entries.emplace_back(row.key, meios::value{});
    }
    return meios::value::make_mapping(std::move(entries));
}

// A deliberately small block-mapping reader: nesting by indentation, one plain scalar per
// leaf, and no other YAML construct. It exists so the expression grammar is provable with
// no third-party dependency and no optional module built — it is a fake, not a parser.
class yaml_parser final : public meios::yaml_parser_handle::parser
{
public:
    meios::yaml_outcome parse(std::string_view bytes, const meios::evaluator_limits &,
                              meios::evaluator_counters &, meios::log_sink &,
                              const meios::source_location &) const override
    {
        const std::vector<yaml_line> rows = yaml_lines(bytes);
        std::size_t at = 0;
        return meios::yaml_outcome{ yaml_mapping(rows, at, rows.empty() ? 0 : rows.front().indent),
                                    meios::yaml_failure::none };
    }
};

inline std::shared_ptr<const meios::yaml_parser_handle> yaml_parser_handle()
{
    return std::make_shared<const meios::yaml_parser_handle>(std::make_unique<yaml_parser>());
}

}

#endif
