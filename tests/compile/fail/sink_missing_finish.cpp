#include <meios/urdf.h>

namespace
{

// Satisfies four of the five model_sink methods but omits finish(), so it must be
// rejected at the model_sink concept gate on parse — never reaching the erased vtable.
struct sink_missing_finish
{
    void on_robot(const meios::robot_info &) {}
    void on_material(const meios::material<double> &) {}
    void on_link(const meios::link<double> &) {}
    void on_joint(const meios::joint<double> &) {}
};

}

void probe(meios::basic_parser<meios::urdf_reader> &parser, sink_missing_finish &sink)
{
    parser.parse("", sink);
}
