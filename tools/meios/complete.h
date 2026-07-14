#ifndef HPP_GUARD_MEIOS_CLI_COMPLETE_H
#define HPP_GUARD_MEIOS_CLI_COMPLETE_H

#include <string>
#include <vector>

namespace meios::cli
{

// The hidden `__complete` hook: given the command-line words with the last as the
// current (partial) word, it prints candidate lines followed by a cobra-style
// directive line, enumerating live only at semantic value positions.
int run_complete(const std::vector<std::string> &words);

}

#endif
