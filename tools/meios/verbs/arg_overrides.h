#ifndef HPP_GUARD_MEIOS_CLI_ARG_OVERRIDES_H
#define HPP_GUARD_MEIOS_CLI_ARG_OVERRIDES_H

#include "meios/diagnostic/log_sink.h"

#include <map>
#include <string>
#include <vector>

namespace meios::cli
{

bool collect_arg_overrides(const std::vector<std::string> &tokens, std::map<std::string, std::string> &args, log_sink &log);

}

#endif
