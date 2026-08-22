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

struct erased_sink
{
    erased_sink() = default;
    erased_sink(const erased_sink &) = default;
    erased_sink &operator=(const erased_sink &) = default;
    erased_sink(erased_sink &&) = default;
    erased_sink &operator=(erased_sink &&) = default;
    virtual ~erased_sink() = default;

    virtual void on_robot(const robot_info &robot) = 0;
    virtual void on_material(const material<double> &mat) = 0;
    virtual void on_link(const link<double> &node) = 0;
    virtual void on_joint(const joint<double> &edge) = 0;
    virtual void finish() = 0;
};

template <typename Sink>
struct sink_adapter final : erased_sink
{
    explicit sink_adapter(Sink &sink) : m_sink(sink) {}

    void on_robot(const robot_info &robot) override { m_sink.on_robot(robot); }
    void on_material(const material<double> &mat) override { m_sink.on_material(mat); }
    void on_link(const link<double> &node) override { m_sink.on_link(node); }
    void on_joint(const joint<double> &edge) override { m_sink.on_joint(edge); }
    void finish() override { m_sink.finish(); }

    // Borrowed, never owned: the adapter is stack-constructed inside parse and the
    // referenced sink provably outlives the one parse_erased call it is passed to.
    Sink &m_sink;
};

template <>
class basic_parser<urdf_reader>
{
public:
    explicit basic_parser(parse_context &context) : m_context(context) {}

    template <typename Sink>
        requires model_sink<Sink>
    void parse(std::string_view source, Sink &sink)
    {
        sink_adapter<Sink> adapter{ sink };
        parse_erased(source, adapter);
    }

private:
    void parse_erased(std::string_view source, erased_sink &sink);

    parse_context &m_context;
};

}

#endif
