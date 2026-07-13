#ifndef HPP_GUARD_MEIOS_MODEL_SINK_FORMAT_READER_H
#define HPP_GUARD_MEIOS_MODEL_SINK_FORMAT_READER_H

namespace meios
{

template <typename R>
concept format_reader = requires
{
    typename R::model;
};

template <typename FormatReader>
class basic_parser;

}

#endif
