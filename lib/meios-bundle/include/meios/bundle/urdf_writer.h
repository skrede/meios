#ifndef HPP_GUARD_MEIOS_BUNDLE_URDF_WRITER_H
#define HPP_GUARD_MEIOS_BUNDLE_URDF_WRITER_H

#include "meios/bundle/result.h"

#include "meios/records/link.h"
#include "meios/records/joint.h"
#include "meios/records/material.h"
#include "meios/records/robot_info.h"

#include "meios/diagnostic/log_sink.h"

#include <map>
#include <string>
#include <memory>
#include <vector>
#include <ostream>
#include <utility>
#include <optional>

namespace meios
{

struct reference_record
{
    std::string original;
    std::optional<std::string> resolved_path;
    bool is_texture;
};

// Move-only type-erased rewrite hook (std::move_only_function is C++23; this
// mirrors byte_reader's C++20-clean unique_ptr-behind-a-vtable idiom). It maps the
// accumulated reference set to an old-URI->new-URI table, so the buffered rewrite is
// deferred to finish() once the full asset set is known; its signature names no
// pugixml type, preserving the single PRIVATE edge.
class reference_planner
{
public:
    struct planner
    {
        planner() = default;
        planner(const planner &) = default;
        planner &operator=(const planner &) = default;
        planner(planner &&) = default;
        planner &operator=(planner &&) = default;
        virtual ~planner() = default;

        virtual std::map<std::string, std::string> plan(const std::vector<reference_record> &refs) = 0;
    };

    reference_planner() = default;
    explicit reference_planner(std::unique_ptr<planner> impl) : m_impl(std::move(impl)) {}

    reference_planner(reference_planner &&) noexcept = default;
    reference_planner &operator=(reference_planner &&) noexcept = default;
    reference_planner(const reference_planner &) = delete;
    reference_planner &operator=(const reference_planner &) = delete;

    ~reference_planner() = default;

    bool valid() const noexcept { return m_impl != nullptr; }

    std::map<std::string, std::string> operator()(const std::vector<reference_record> &refs) const
    {
        return m_impl->plan(refs);
    }

private:
    std::unique_ptr<planner> m_impl;
};

// Shared record-driven emitter: flatten drives it with an identity rewrite; bundle
// supplies a required package name and the planner hook, buffering until finish().
class urdf_writer
{
public:
    urdf_writer(std::ostream &out, log_sink &log);
    urdf_writer(std::ostream &out, log_sink &log, std::string bundle_name, reference_planner plan_cb);

    void on_robot(const robot_info &robot);
    void on_material(const material<double> &mat);
    void on_link(const link<double> &node);
    void on_joint(const joint<double> &edge);
    void finish();

    const emit_result &status() const;

private:
    std::ostream &m_out;
    log_sink &m_log;
    std::string m_bundle_name;
    emit_result m_result;
    std::vector<reference_record> m_refs;
    reference_planner m_plan_cb;
};

}

#endif
