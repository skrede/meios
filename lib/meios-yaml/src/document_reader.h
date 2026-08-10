#ifndef HPP_GUARD_MEIOS_YAML_DOCUMENT_READER_H
#define HPP_GUARD_MEIOS_YAML_DOCUMENT_READER_H

#include "meios/xacro/evaluator_limits.h"
#include "meios/xacro/yaml_parser_handle.h"

#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/source_location.h"

#include <string_view>

namespace meios::detail
{

class yaml_document_reader final : public yaml_parser_handle::parser
{
public:
    yaml_outcome parse(std::string_view bytes, const evaluator_limits &limits,
                       evaluator_counters &counters, log_sink &log,
                       const source_location &at) const override;
};

}

#endif
