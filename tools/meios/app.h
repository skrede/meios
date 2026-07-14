#ifndef HPP_GUARD_MEIOS_CLI_APP_H
#define HPP_GUARD_MEIOS_CLI_APP_H

#include <string>
#include <vector>

namespace meios::cli
{

// One registered subcommand as seen by the parser, named without any CLI11 type so
// the drift test can compare the parser surface to the completion surface without
// linking CLI11 (which stays PRIVATE to the tool).
struct parser_surface_entry
{
    std::string name;
    std::vector<std::string> flags;
    std::vector<std::string> positionals;
};

int run(int argc, char **argv);

std::vector<parser_surface_entry> registered_surface();

}

#endif
