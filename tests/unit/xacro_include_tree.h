#ifndef HPP_GUARD_MEIOS_UNIT_XACRO_INCLUDE_TREE_H
#define HPP_GUARD_MEIOS_UNIT_XACRO_INCLUDE_TREE_H

#include "xacro_read_probe.h"

#include <meios/xacro.h>

#include <meios/io/scratch_dir.h>
#include <meios/io/source_stack.h>

#include <meios/diagnostic/log_sink.h>

#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <string_view>
#include <system_error>

namespace include_tree
{

using expansion_result = meios::expected<meios::expansion, meios::expansion_error>;

class tree
{
public:
    tree()
            : m_dir(xacro_probe::fresh_dir())
    {
    }

    const std::filesystem::path &root() const
    {
        return m_dir.path();
    }

    std::filesystem::path write(const std::string &relative, std::string_view text) const
    {
        const std::filesystem::path file = root() / relative;
        std::filesystem::create_directories(file.parent_path());
        std::ofstream(file, std::ios::binary) << text;
        return file;
    }

private:
    meios::scratch_dir m_dir;
};

class working_directory
{
public:
    explicit working_directory(const std::filesystem::path &to)
            : m_saved(std::filesystem::current_path())
    {
        std::filesystem::current_path(to);
    }

    ~working_directory()
    {
        std::error_code ignored;
        std::filesystem::current_path(m_saved, ignored);
    }

    working_directory(const working_directory &) = delete;
    working_directory(working_directory &&)      = delete;

    working_directory &operator=(const working_directory &) = delete;
    working_directory &operator=(working_directory &&)      = delete;

private:
    std::filesystem::path m_saved;
};

inline std::string robot(std::string_view body)
{
    return "<robot name=\"probe\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">" + std::string(body) + "</robot>";
}

inline std::string include(std::string_view filename)
{
    return "<xacro:include filename=\"" + std::string(filename) + "\"/>";
}

inline std::string binds(std::string_view value)
{
    return robot("<xacro:property name=\"leaf\" value=\"" + std::string(value) + "\"/>");
}

inline std::string reads_leaf(std::string_view filename)
{
    return robot(include(filename) + "<link name=\"${leaf}\"/>");
}

inline expansion_result expand_text(std::string_view text, const std::filesystem::path &document, meios::source_stack &sources, meios::log_sink &log)
{
    meios::eval_scope scope;
    return meios::expand(text, scope, sources, document, meios::expansion_limits{}, log);
}

inline std::string read_file(const std::filesystem::path &document)
{
    std::ifstream in(document, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

inline expansion_result expand_file(const std::filesystem::path &document, meios::source_stack &sources, meios::log_sink &log)
{
    return expand_text(read_file(document), document, sources, log);
}

inline bool renders_link(const expansion_result &out, std::string_view name)
{
    return out.has_value() && out->document.find("name=\"" + std::string(name) + '"') != std::string::npos;
}

}

#endif
