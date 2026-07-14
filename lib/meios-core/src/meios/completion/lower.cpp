#include "meios/completion/completion_model.h"

#include <utility>

namespace meios
{

completion_model lower(const std::vector<command_spec> &table)
{
    completion_model model{ "meios", {} };
    for(const command_spec &cmd : table)
    {
        completion_verb verb{ cmd.name, cmd.description, {}, {} };
        for(const flag_spec &flag : cmd.flags)
            verb.options.push_back({ flag.token, flag.description,
                                     flag.kind == flag_kind::value, flag.dynamic });
        for(const positional_spec &pos : cmd.positionals)
            verb.arguments.push_back({ pos.name, pos.description, pos.dynamic });
        model.verbs.push_back(std::move(verb));
    }
    return model;
}

}
