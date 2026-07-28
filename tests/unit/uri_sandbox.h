#ifndef HPP_GUARD_MEIOS_UNIT_URI_SANDBOX_H
#define HPP_GUARD_MEIOS_UNIT_URI_SANDBOX_H

#include <string>
#include <random>
#include <fstream>
#include <sstream>
#include <iterator>
#include <filesystem>
#include <string_view>
#include <system_error>

namespace uri
{

inline std::string unique_stem()
{
    std::random_device entropy;
    std::ostringstream stem;
    stem << "meios-uri-" << std::hex << entropy() << entropy();
    return stem.str();
}

inline void seed_file(const std::filesystem::path &file)
{
    std::filesystem::create_directories(file.parent_path());
    std::ofstream(file) << "seed\n";
}

inline void replace_all(std::string &text, std::string_view token, const std::string &with)
{
    for(std::string::size_type at = text.find(token); at != std::string::npos;
        at = text.find(token, at + with.size()))
        text.replace(at, token.size(), with);
}

inline std::string slurp(const std::filesystem::path &path)
{
    std::ifstream in(path);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

// A uri fixture is a template rather than a document, because a row about an absolute path
// cannot name one that exists on every machine: it names @ROOT@ for a directory this type
// registers as a package root and writes the document into, and @OUTSIDE@ for one registered
// nowhere, and the tree lives only as long as the object.
class sandbox
{
public:
    sandbox()
        : m_base(std::filesystem::temp_directory_path() / unique_stem()),
          m_root(m_base / "root"),
          m_outside(m_base / "outside")
    {
        seed_file(m_root / "meshes" / "base.stl");
        seed_file(m_root / "spaced dir" / "base.stl");
        seed_file(m_root / "somepkg" / "meshes" / "base.stl");
        seed_file(m_root / "somepkg" / "textures" / "skin.png");
        seed_file(m_outside / "stray.stl");
    }

    sandbox(const sandbox &) = delete;

    sandbox &operator=(const sandbox &) = delete;

    ~sandbox()
    {
        std::error_code ec;
        std::filesystem::remove_all(m_base, ec);
    }

    const std::filesystem::path &root() const { return m_root; }

    // RFC 8089 appendix E.2 spells a drive path with a leading separator, so a row that has to
    // put something ahead of the root — an authority — needs the root as a URI path component
    // rather than as a native one, or the two run together on a drive-letter host.
    std::string root_uri_path() const
    {
        const std::string text = m_root.generic_string();
        return text.starts_with("/") ? text : "/" + text;
    }

    std::filesystem::path write_document(std::string text) const
    {
        replace_all(text, "@ROOTURI@", root_uri_path());
        replace_all(text, "@ROOT@", m_root.generic_string());
        replace_all(text, "@OUTSIDE@", m_outside.generic_string());
        const std::filesystem::path document = m_root / "document.urdf";
        std::ofstream(document) << text;
        return document;
    }

private:
    std::filesystem::path m_base;
    std::filesystem::path m_root;
    std::filesystem::path m_outside;
};

}

#endif
