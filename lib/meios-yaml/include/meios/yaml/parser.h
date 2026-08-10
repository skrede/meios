#ifndef HPP_GUARD_MEIOS_YAML_PARSER_H
#define HPP_GUARD_MEIOS_YAML_PARSER_H

#include "meios/xacro/yaml_parser_handle.h"

#include <memory>

namespace meios
{

// The module's whole public surface: one factory whose signature names core types only, so a
// consumer never sees the parsing library this module carries.
std::shared_ptr<const yaml_parser_handle> make_yaml_parser();

}

#endif
