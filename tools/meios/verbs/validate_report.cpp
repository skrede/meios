#include "validate.h"
#include "json_escape.h"

#include <string>
#include <cstddef>
#include <sstream>
#include <ostream>

namespace meios::cli
{

namespace
{

void write_messages(std::ostream &out, const std::vector<std::string> &messages)
{
    for(std::size_t i = 0; i < messages.size(); ++i)
        out << (i == 0 ? "" : ",") << '"' << json_escape(messages[i]) << '"';
}

void write_class(std::ostream &out, const validation_finding &finding)
{
    out << "{\"code\":" << finding.code << ",\"class\":\"" << finding.klass << "\",\"messages\":[";
    write_messages(out, finding.messages);
    out << "]}";
}

}

std::string format_text(const validation_report &report)
{
    std::ostringstream out;
    const int code = exit_code(report);
    out << "validation: " << (code == 0 ? "OK" : "FAIL") << " (exit " << code << ")\n";
    for(const validation_finding &finding : report.findings)
    {
        out << '[' << finding.code << "] " << finding.klass << ":\n";
        for(const std::string &message : finding.messages)
            out << "  - " << message << '\n';
    }
    return out.str();
}

std::string format_json(const validation_report &report)
{
    std::ostringstream out;
    const int code = exit_code(report);
    out << "{\"status\":\"" << (code == 0 ? "ok" : "fail") << "\",\"exit_code\":" << code
        << ",\"classes\":[";
    for(std::size_t i = 0; i < report.findings.size(); ++i)
    {
        if(i != 0)
            out << ',';
        write_class(out, report.findings[i]);
    }
    out << "]}\n";
    return out.str();
}

}
