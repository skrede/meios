#include "validate.h"

#include <string>
#include <cstdio>
#include <cstddef>
#include <sstream>
#include <ostream>

namespace meios::cli
{

namespace
{

// Escapes a string for a JSON double-quoted scalar: the two structural characters
// plus every C0 control byte (RFC 8259 §7 forbids raw control bytes in strings).
std::string json_escape(const std::string &text)
{
    std::string out;
    for(const char raw : text)
    {
        const unsigned char byte = static_cast<unsigned char>(raw);
        if(raw == '"' || raw == '\\')
            out += { '\\', raw };
        else if(byte < 0x20)
        {
            char buffer[8];
            std::snprintf(buffer, sizeof(buffer), "\\u%04x", byte);
            out += buffer;
        }
        else
            out += raw;
    }
    return out;
}

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
