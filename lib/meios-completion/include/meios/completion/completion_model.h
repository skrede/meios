#ifndef HPP_GUARD_MEIOS_COMPLETION_COMPLETION_MODEL_H
#define HPP_GUARD_MEIOS_COMPLETION_COMPLETION_MODEL_H

#include "meios/completion/command_table.h"

#include <string>
#include <vector>

namespace meios
{

struct completion_option
{
    std::string token;
    std::string description;
    bool takes_argument;
    bool dynamic_argument;
};

struct completion_argument
{
    std::string name;
    std::string description;
    bool dynamic;
};

struct completion_verb
{
    std::string name;
    std::string description;
    std::vector<completion_option> options;
    std::vector<completion_argument> arguments;
};

// The shell-neutral IR the three emitters serialize. It records which positions are
// dynamic so every emitter fires `meios __complete` at the same places.
struct completion_model
{
    std::string program;
    std::vector<completion_verb> verbs;
};

completion_model lower(const std::vector<command_spec> &table);

}

#endif
