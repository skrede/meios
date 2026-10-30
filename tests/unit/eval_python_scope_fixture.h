#ifndef HPP_GUARD_MEIOS_UNIT_EVAL_PYTHON_SCOPE_FIXTURE_H
#define HPP_GUARD_MEIOS_UNIT_EVAL_PYTHON_SCOPE_FIXTURE_H

#include "xacro_include_tree.h"

#include <meios/eval/python_evaluator.h>

#include <meios/xacro.h>
#include <meios/xacro/evaluator_handle.h>

#include <meios/io/source_stack.h>

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <memory>
#include <string>
#include <cstddef>
#include <optional>
#include <filesystem>
#include <string_view>

namespace scope_probe
{

struct loaded
{
    include_tree::expansion_result out;
    std::size_t errors;
};

inline std::shared_ptr<meios::evaluator_handle> python()
{
    return std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
}

inline std::string property(std::string_view name, std::string_view value)
{
    return "<xacro:property name=\"" + std::string(name) + "\" value=\"" + std::string(value) + "\"/>";
}

inline std::string include_as(std::string_view filename, std::string_view space)
{
    return "<xacro:include filename=\"" + std::string(filename) + "\" ns=\"" + std::string(space) + "\"/>";
}

inline std::string link(std::string_view name)
{
    return "<link name=\"" + std::string(name) + "\"/>";
}

inline loaded load_with(const include_tree::tree &held, std::string_view body, const std::shared_ptr<meios::evaluator_handle> &backend)
{
    const std::filesystem::path document = held.write("top.xacro", include_tree::robot(body));
    std::size_t errors                   = 0;
    meios::log_sink_f counting([&errors](meios::level lvl, const auto &...) { errors += lvl == meios::level::error ? 1 : 0; });
    meios::eval_scope scope;
    meios::source_stack sources{};
    include_tree::expansion_result out =
            meios::expand(include_tree::read_file(document), scope, sources, document, meios::expansion_limits{}, meios::eval_policy::fail, backend, counting);
    return loaded{std::move(out), errors};
}

inline loaded load(std::string_view body)
{
    const include_tree::tree held;
    return load_with(held, body, python());
}

inline bool renders(const loaded &got, std::string_view name)
{
    return got.errors == 0 && include_tree::renders_link(got.out, name);
}

inline bool refuses(const loaded &got, std::string_view naming)
{
    return !got.out.has_value() && got.out.error().message.find(naming) != std::string::npos;
}

inline bool refuses_once(const loaded &got, std::string_view naming)
{
    return refuses(got, naming) && got.errors == 1;
}

}

#endif
