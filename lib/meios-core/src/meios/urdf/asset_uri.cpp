#include "asset_uri.h"

#include "meios/io/uri_decode.h"

#include <string>
#include <cstddef>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios::detail
{

namespace
{

bool alphabetic(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool scheme_body(char c)
{
    return alphabetic(c) || (c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.';
}

// RFC 8089 appendix E.2 spells an authority-less DOS drive as file:///c:/path, so the
// separator ahead of the drive letter belongs to the URI grammar and not to the path.
// Recognizing it by shape rather than by platform keeps one rule on all three. The separator
// after the colon is what tells a drive apart from a path component that merely contains one.
std::string_view strip_drive_separator(std::string_view text)
{
    if(text.size() >= 4 && text[0] == '/' && text[2] == ':' && alphabetic(text[1]) && (text[3] == '/' || text[3] == '\\'))
        return text.substr(1);
    return text;
}

}

// RFC 3986 section 3.1 spells a scheme as ALPHA *( ALPHA / DIGIT / "+" / "-" / "." ). The
// alphabetic first character is the stricter of the two readings, and it is what keeps a
// filename opening with a digit and two slashes from being read as a scheme.
std::optional<std::string_view> scheme_of(std::string_view uri)
{
    const std::size_t mark = uri.find("://");
    if(mark == 0 || mark == std::string_view::npos)
        return std::nullopt;
    const std::string_view head = uri.substr(0, mark);
    if(!alphabetic(head.front()))
        return std::nullopt;
    for(char c : head)
    {
        if(!scheme_body(c))
            return std::nullopt;
    }
    return head;
}

// The test is a scheme followed by two slashes, never a bare colon before the first
// separator: a drive-letter path carries no double slash after its colon and so reaches the
// filesystem branch below. Refusing on the colon alone would misread every Windows absolute
// path as a one-letter scheme and break one of the three supported platforms.
asset_uri_form classify_asset_uri(std::string_view uri)
{
    const std::optional<std::string_view> scheme = scheme_of(uri);
    if(scheme == "package")
        return asset_uri_form::package;
    if(scheme == "file")
        return asset_uri_form::absolute;
    if(scheme)
        return asset_uri_form::foreign_scheme;
    return std::filesystem::path(uri).is_absolute() ? asset_uri_form::absolute
                                                    : asset_uri_form::relative;
}

// RFC 8089 section 2 places the authority between the scheme's two separators and the next
// separator. The two-character drive shape is exempted here rather than at the judging site
// because reading it as a host would refuse file://c:/path, a spelling this contract publishes
// as accepted; deciding it by the shape of the text keeps one rule on all three platforms.
std::string_view file_authority(std::string_view uri)
{
    constexpr std::string_view prefix = "file://";
    if(!uri.starts_with(prefix))
        return {};
    const std::string_view rest = uri.substr(prefix.size());
    const std::size_t end = rest.find('/');
    const std::string_view head = end == std::string_view::npos ? rest : rest.substr(0, end);
    if(head.size() == 2 && head[1] == ':' && alphabetic(head[0]))
        return {};
    return head;
}

std::string file_uri_to_path(std::string_view uri)
{
    constexpr std::string_view prefix = "file://";
    if(uri.starts_with(prefix))
        uri.remove_prefix(prefix.size());
    return percent_decode(strip_drive_separator(uri));
}

}
