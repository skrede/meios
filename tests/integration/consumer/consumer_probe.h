#ifndef HPP_GUARD_CONSUMER_CONSUMER_PROBE_H
#define HPP_GUARD_CONSUMER_CONSUMER_PROBE_H

#include <meios/model.h>

#include <string>
#include <vector>
#include <optional>
#include <string_view>

namespace consumer
{

// A model_sink defined entirely outside the meios source tree. It is deliberately
// none of the sinks the reader ever explicitly instantiated, so it can only link if
// the parse boundary is genuinely type-erased in the installed package.
struct record_counter
{
    int robots = 0;
    int materials = 0;
    int links = 0;
    int joints = 0;
    bool finished = false;

    void on_robot(const meios::robot_info &) { ++robots; }
    void on_material(const meios::material<double> &) { ++materials; }
    void on_link(const meios::link<double> &) { ++links; }
    void on_joint(const meios::joint<double> &) { ++joints; }
    void finish() { finished = true; }
};

inline constexpr std::string_view probe_urdf = R"(<?xml version="1.0"?>
<robot name="counter_probe">
  <material name="grey">
    <color rgba="0.5 0.5 0.5 1.0"/>
  </material>
  <link name="base_link"/>
  <link name="tool"/>
  <joint name="base_to_tool" type="fixed">
    <parent link="base_link"/>
    <child link="tool"/>
    <origin xyz="0 0 0.1" rpy="0 0 0"/>
  </joint>
</robot>
)";

struct diagnostic_record
{
    meios::level lvl;
    meios::diagnostic_code code;
    std::string message;

    std::optional<meios::operation_failure> cause;
};

// A diagnostic sink living entirely in the consumer's tree: it lifts every library
// diagnostic straight into the consumer's own diagnostic_record. The four-argument
// overload is the one that carries the typed diagnostic_code across the install
// boundary, so a code-bearing diagnostic lands with its code intact; the five-argument
// one carries the operation_failure behind a failure, and without it a library that
// reported a structured cause would be indistinguishable here from one that did not.
class diagnostic_lift
{
public:
    explicit diagnostic_lift(std::vector<diagnostic_record> &out) : m_out(out) {}

    void operator()(meios::level lvl, const std::string &message)
    {
        m_out.push_back({ lvl, meios::diagnostic_code::unspecified, message, std::nullopt });
    }

    void operator()(meios::level lvl, const meios::source_location &, const std::string &message)
    {
        m_out.push_back({ lvl, meios::diagnostic_code::unspecified, message, std::nullopt });
    }

    void operator()(meios::level lvl, meios::diagnostic_code code, const meios::source_location &,
                    const std::string &message)
    {
        m_out.push_back({ lvl, code, message, std::nullopt });
    }

    void operator()(meios::level lvl, meios::diagnostic_code code, const meios::source_location &,
                    const meios::operation_failure &cause, const std::string &message)
    {
        m_out.push_back({ lvl, code, message, cause });
    }

private:
    std::vector<diagnostic_record> &m_out;
};

// Defined in the further translation units of this same project, so every exercise runs
// under the one command the continuous-integration jobs already invoke.
int run_facade();
int run_memory_source();
int run_asset_reads();

}

#endif
