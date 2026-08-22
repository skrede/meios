#ifndef HPP_GUARD_MEIOS_UNIT_XACRO_SUBST_FIXTURE_H
#define HPP_GUARD_MEIOS_UNIT_XACRO_SUBST_FIXTURE_H

#include <meios/xacro.h>

#include <meios/io/source_stack.h>
#include <meios/io/memory_source.h>
#include <meios/io/directory_source.h>

#include <meios/expected.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>
#include <meios/diagnostic/expansion_error.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstdlib>
#include <fstream>
#include <utility>
#include <filesystem>
#include <string_view>

namespace
{

struct captured_log
{
    std::vector<std::pair<meios::level, std::string>> &records;

    void operator()(meios::level lvl, const std::string &message)
    {
        records.push_back({ lvl, message });
    }

    void operator()(meios::level lvl, const meios::source_location &, const std::string &message)
    {
        records.push_back({ lvl, message });
    }

    void operator()(meios::level lvl, meios::diagnostic_code, const meios::source_location &,
                    const std::string &message)
    {
        records.push_back({ lvl, message });
    }
};

using substitution_result = meios::expected<meios::substitution, meios::expansion_error>;

bool any_contains(const std::vector<std::pair<meios::level, std::string>> &records,
                  std::string_view needle)
{
    for(const std::pair<meios::level, std::string> &entry : records)
        if(entry.second.find(needle) != std::string::npos)
            return true;
    return false;
}

// A refusal that names no file carries a record no site ever composed, which is how a
// missed refusal site would otherwise pass as a code assertion alone.
bool locates_a_file(const substitution_result &refused)
{
    return !refused.error().loc.file.empty();
}

int records_at(const std::vector<std::pair<meios::level, std::string>> &records, meios::level lvl)
{
    int count = 0;
    for(const std::pair<meios::level, std::string> &entry : records)
        if(entry.first == lvl)
            ++count;
    return count;
}

}

#endif
