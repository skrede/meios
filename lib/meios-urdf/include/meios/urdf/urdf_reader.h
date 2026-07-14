#ifndef HPP_GUARD_MEIOS_URDF_URDF_READER_H
#define HPP_GUARD_MEIOS_URDF_URDF_READER_H

#include "meios/urdf/parse_context.h"

#include "meios/sink/model_sink.h"
#include "meios/sink/format_reader.h"

#include "meios/model/tree.h"

#include "meios/math/rotations.h"

#include <string_view>

namespace meios
{

struct urdf_reader
{
    using model = tree<double, rpy<double>>;
};

template <>
class basic_parser<urdf_reader>
{
public:
    explicit basic_parser(parse_context &context) : m_context(context) {}

    template <typename Sink>
        requires model_sink<Sink>
    void parse(std::string_view source, Sink &sink);

private:
    parse_context &m_context;
};

}

#endif
