#ifndef HPP_GUARD_MEIOS_XACRO_ARG_SCAN_H
#define HPP_GUARD_MEIOS_XACRO_ARG_SCAN_H

#include "meios/diagnostic/log_sink.h"

#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios
{

struct arg_declaration
{
    std::string name;
    std::optional<std::string> default_value;
};

// Enumerates a document's args: every `<xacro:arg name= default=>` declaration
// and every `$(arg NAME)` reference, de-duplicated (first seen wins) in document
// order. The signature exposes no XML-library type so the CLI can call it directly.
std::vector<arg_declaration> scan_args(const std::filesystem::path &path, log_sink &log);

std::vector<arg_declaration> scan_args(std::string_view source, log_sink &log);

}

#endif
