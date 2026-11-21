#include "arg_overrides.h"

#include "meios/xacro/python_space.h"

#include "meios/diagnostic/level.h"

#include <map>
#include <string>
#include <vector>
#include <string_view>

namespace meios::cli
{

namespace
{

std::string_view unbound_reason(std::string_view name, std::string_view value)
{
    if(name.empty())
        return "its name is empty";
    if(value.empty())
        return "its value is empty";
    if(name.size() > 1 && name[0] == '_' && name[1] != '_')
        return "a name with one leading underscore is read as a parameter, not an argument";
    return {};
}

bool bind_override(const std::string &token, std::map<std::string, std::string> &args, log_sink &log)
{
    const std::string::size_type split = token.find(":=");
    if(split == std::string::npos)
    {
        log.log(level::error, "malformed argument override '" + token + "'; expected key:=value");
        return false;
    }
    const std::string_view name   = detail::python_stripped(std::string_view(token).substr(0, split));
    const std::string_view value  = detail::python_stripped(std::string_view(token).substr(split + 2));
    const std::string_view reason = unbound_reason(name, value);
    if(reason.empty())
        args[std::string(name)] = std::string(value);
    else
        log.log(level::warn, "argument override '" + token + "' binds nothing: " + std::string(reason));
    return true;
}

}

// Stripping belongs to reading a command line, so a program's load_options::args binds the text
// it was handed as written.
bool collect_arg_overrides(const std::vector<std::string> &tokens, std::map<std::string, std::string> &args, log_sink &log)
{
    bool ok = true;
    for(const std::string &token : tokens)
        ok = bind_override(token, args, log) && ok;
    return ok;
}

}
