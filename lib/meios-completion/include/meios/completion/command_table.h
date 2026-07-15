#ifndef HPP_GUARD_MEIOS_COMPLETION_COMMAND_TABLE_H
#define HPP_GUARD_MEIOS_COMPLETION_COMMAND_TABLE_H

#include <string>
#include <vector>

namespace meios
{

enum class flag_kind
{
    boolean, // present or absent, takes no argument
    value,   // takes a single argument
};

struct positional_spec
{
    std::string name;
    std::string description;
    // A dynamic position is enumerated live by `meios __complete`, not from this table.
    bool dynamic;
    // A variadic position greedily collects every remaining trailing token.
    bool variadic;
};

struct flag_spec
{
    std::string token;
    std::string description;
    flag_kind kind;
    // Whether the flag's argument is enumerated live rather than a fixed set.
    bool dynamic;
};

struct command_spec
{
    std::string id;
    std::string name;
    std::string description;
    std::vector<flag_spec> flags;
    std::vector<positional_spec> positionals;
};

// The single source of truth for every verb, flag, and description. The CLI11
// parser and the completion emitters both derive from this one table so neither
// can drift from the other.
const std::vector<command_spec> &cli_table();

}

#endif
