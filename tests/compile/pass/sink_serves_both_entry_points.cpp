#include <meios/urdf.h>

#include <filesystem>

namespace
{

struct scene_builder
{
    void on_robot(const meios::robot_info &) {}
    void on_material(const meios::material<double> &) {}
    void on_link(const meios::link<double> &) {}
    void on_joint(const meios::joint<double> &) {}
    void finish() {}
};

struct sink_missing_finish
{
    void on_robot(const meios::robot_info &) {}
    void on_material(const meios::material<double> &) {}
    void on_link(const meios::link<double> &) {}
    void on_joint(const meios::joint<double> &) {}
};

template <typename Sink>
concept facade_accepts = requires(const std::filesystem::path &path,
                                  const meios::load_options &opts, Sink &sink, meios::log_sink &log)
{
    meios::load_into(path, opts, sink, log);
};

static_assert(meios::model_sink<scene_builder>);
static_assert(!meios::model_sink<sink_missing_finish>);

// The rejection has to come from the declaration rather than the body: an unconstrained
// facade would still deduce for a non-sink and satisfy this, so dropping the concept
// constraint breaks this translation unit.
static_assert(facade_accepts<scene_builder>);
static_assert(!facade_accepts<sink_missing_finish>);

}

// One sink type, unedited, reaches the buffer-driven parse entry point and the
// path-driven facade; neither asks it for anything the other does not.
void serves_both(meios::basic_parser<meios::urdf_reader> &parser,
                 const std::filesystem::path &path, meios::log_sink &log)
{
    scene_builder sink;
    parser.parse("<robot name=\"probe\"/>", sink);
    const meios::load_summary summary = meios::load_into(path, meios::load_options{}, sink, log);
    static_cast<void>(summary.claims);
}
