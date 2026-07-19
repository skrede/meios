#ifndef HPP_GUARD_MEIOS_CLI_VALIDATE_H
#define HPP_GUARD_MEIOS_CLI_VALIDATE_H

#include "meios/model/model.h"

#include "meios/diagnostic/log_sink.h"

#include <string>
#include <vector>
#include <cstddef>
#include <filesystem>

namespace meios::cli
{

// One failing validation class: its exit code, a stable machine-readable name,
// and the diagnostics the responsible pass produced. The four classes and their
// severity order are locked; the exit code is the most-severe (lowest) failing
// code, so a lower code always dominates a higher one.
struct validation_finding
{
    int code;
    std::string klass;
    std::vector<std::string> messages;
};

struct validation_report
{
    std::vector<validation_finding> findings;
};

// The most-severe failing code (lowest of the present findings), or 0 when the
// report is clean. Exit code and reported status therefore always agree.
int exit_code(const validation_report &report);

// Records every diagnostic a pass emits at error level so the caller can attribute
// a whole class to the single library pass that raised it, never to message text.
class recording_sink final : public log_sink
{
public:
    using log_sink::log;

    recording_sink() : m_messages() {}

    void log(level lvl, const std::string &message) override;

    void log(level lvl, const source_location &location, const std::string &message) override;

    bool had_error() const { return !m_messages.empty(); }

    const std::vector<std::string> &messages() const { return m_messages; }

private:
    std::vector<std::string> m_messages;
};

// Runs the staged library passes and buckets each failure by the pass that raised
// it (design A): well-formedness/strictness (1), xacro expansion (4), topology (2),
// and the named schema rules (3).
validation_report classify(const std::filesystem::path &path,
                           const std::vector<std::filesystem::path> &roots);

// Appends the single named schema rule's violations (a revolute or prismatic joint
// must declare a <limit>) to the sink as errors. This is the only rule enforced.
void schema_check(const model<double> &robot, log_sink &log);

std::string format_text(const validation_report &report);

std::string format_json(const validation_report &report);

}

#endif
