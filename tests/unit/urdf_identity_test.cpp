#include <meios/urdf.h>
#include <meios/model.h>
#include <meios/io.h>
#include <meios/xacro.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>

namespace
{

struct note
{
    meios::level           lvl;
    meios::diagnostic_code code;
    meios::source_location loc;
    std::string            message;
};

struct recorder
{
    std::vector<note> &notes;

    void operator()(meios::level lvl, const std::string &message)
    {
        notes.push_back({ lvl, meios::diagnostic_code::unspecified, {}, message });
    }

    void operator()(meios::level lvl, const meios::source_location &loc, const std::string &message)
    {
        notes.push_back({ lvl, meios::diagnostic_code::unspecified, loc, message });
    }

    void operator()(meios::level lvl, meios::diagnostic_code code, const meios::source_location &loc,
                    const std::string &message)
    {
        notes.push_back({ lvl, code, loc, message });
    }
};

struct outcome
{
    std::vector<note> notes;
    bool              loaded;
};

outcome load_with(const std::string &name, const meios::load_options &opts)
{
    std::vector<note> notes;
    meios::log_sink_f capture{ recorder{ notes } };
    const bool loaded =
        meios::load(std::filesystem::path{ MEIOS_URDF_FIXTURE_DIR } / "profile" / name, opts,
                    capture)
            .has_value();
    return { std::move(notes), loaded };
}

outcome load_fixture(const std::string &name, meios::strictness strict)
{
    meios::load_options opts;
    opts.strict = strict;
    return load_with(name, opts);
}

bool located(const std::vector<note> &notes, meios::diagnostic_code code)
{
    return std::any_of(notes.begin(), notes.end(), [code](const note &n) {
        return n.lvl == meios::level::error && n.code == code && !n.loc.file.empty()
            && n.loc.line > 0;
    });
}

bool naming(const std::vector<note> &notes, meios::diagnostic_code code, const std::string &needle)
{
    return std::any_of(notes.begin(), notes.end(), [code, &needle](const note &n) {
        return n.code == code && n.message.find(needle) != std::string::npos;
    });
}

struct refusal
{
    std::string            fixture;
    meios::diagnostic_code code;
};

std::vector<refusal> refusals()
{
    return { { "link_name_absent.urdf", meios::diagnostic_code::empty_name },
             { "link_name_blank.urdf", meios::diagnostic_code::empty_name },
             { "joint_name_absent.urdf", meios::diagnostic_code::empty_name },
             { "duplicate_link_name.urdf", meios::diagnostic_code::duplicate_name },
             { "duplicate_joint_name.urdf", meios::diagnostic_code::duplicate_name },
             { "undeclared_parent_link.urdf", meios::diagnostic_code::undeclared_link },
             { "parent_link_attr_absent.urdf", meios::diagnostic_code::empty_name },
             { "dangling_mimic.urdf", meios::diagnostic_code::dangling_mimic },
             { "self_mimic.urdf", meios::diagnostic_code::dangling_mimic },
             { "duplicate_material_name.urdf", meios::diagnostic_code::duplicate_name },
             { "zero_links.urdf", meios::diagnostic_code::no_links },
             { "visual_material_unnamed.urdf", meios::diagnostic_code::empty_name } };
}

}

TEST_CASE("an absent, empty or blank name refuses the element that carries it", "[urdf][identity]")
{
    const outcome absent = load_fixture("link_name_absent.urdf", meios::strictness::fail);
    REQUIRE_FALSE(absent.loaded);
    REQUIRE(located(absent.notes, meios::diagnostic_code::empty_name));
    REQUIRE(naming(absent.notes, meios::diagnostic_code::empty_name, "<link>"));

    const outcome joint = load_fixture("joint_name_absent.urdf", meios::strictness::fail);
    REQUIRE_FALSE(joint.loaded);
    REQUIRE(naming(joint.notes, meios::diagnostic_code::empty_name, "<joint>"));
}

TEST_CASE("a duplicate link, joint or material name names itself at its second occurrence",
          "[urdf][identity]")
{
    const outcome links = load_fixture("duplicate_link_name.urdf", meios::strictness::fail);
    REQUIRE_FALSE(links.loaded);
    REQUIRE(naming(links.notes, meios::diagnostic_code::duplicate_name, "'base_link'"));
    REQUIRE(links.notes.front().loc.line == 4);

    REQUIRE(naming(load_fixture("duplicate_joint_name.urdf", meios::strictness::fail).notes,
                   meios::diagnostic_code::duplicate_name, "'shoulder'"));
    REQUIRE(naming(load_fixture("duplicate_material_name.urdf", meios::strictness::fail).notes,
                   meios::diagnostic_code::duplicate_name, "'grey'"));
}

TEST_CASE("identity is exact bytes, so case and trailing space keep names apart", "[urdf][identity]")
{
    const outcome distinct = load_fixture("case_distinct_names.urdf", meios::strictness::fail);
    REQUIRE(distinct.loaded);
    REQUIRE(std::none_of(distinct.notes.begin(), distinct.notes.end(),
                         [](const note &n) { return n.lvl == meios::level::error; }));
}

TEST_CASE("a joint naming a link the document does not declare is refused", "[urdf][identity]")
{
    const outcome parent = load_fixture("undeclared_parent_link.urdf", meios::strictness::fail);
    REQUIRE_FALSE(parent.loaded);
    REQUIRE(naming(parent.notes, meios::diagnostic_code::undeclared_link, "parent link 'ghost_link'"));

    const outcome absent = load_fixture("parent_link_attr_absent.urdf", meios::strictness::fail);
    REQUIRE_FALSE(absent.loaded);
    REQUIRE(naming(absent.notes, meios::diagnostic_code::empty_name, "no parent link"));
}

TEST_CASE("a mimic naming an undeclared joint or its own joint is refused", "[urdf][identity]")
{
    REQUIRE(naming(load_fixture("dangling_mimic.urdf", meios::strictness::fail).notes,
                   meios::diagnostic_code::dangling_mimic, "'never_declared'"));
    REQUIRE(naming(load_fixture("self_mimic.urdf", meios::strictness::fail).notes,
                   meios::diagnostic_code::dangling_mimic, "mimics itself"));
}

TEST_CASE("a document with no links, and a visual material naming none, are refused",
          "[urdf][identity]")
{
    REQUIRE(located(load_fixture("zero_links.urdf", meios::strictness::fail).notes,
                    meios::diagnostic_code::no_links));
    REQUIRE(located(load_fixture("visual_material_unnamed.urdf", meios::strictness::fail).notes,
                    meios::diagnostic_code::empty_name));
}

TEST_CASE("every identity refusal fires at all three document-validity settings",
          "[urdf][identity][structural]")
{
    for(const refusal &r : refusals())
        // A structural refusal answers to no setting, so each case runs against all three.
        for(const meios::strictness strict :
            { meios::strictness::fail, meios::strictness::warn, meios::strictness::skip })
        {
            INFO(r.fixture);
            const outcome result = load_fixture(r.fixture, strict);
            REQUIRE_FALSE(result.loaded);
            REQUIRE(located(result.notes, r.code));
        }
}

TEST_CASE("a permissive graph policy no longer reopens a dangling link reference",
          "[urdf][identity][structural]")
{
    for(const meios::topology_policy policy : { meios::topology_policy::fail,
                                                meios::topology_policy::warn,
                                                meios::topology_policy::skip })
    {
        meios::load_options opts;
        opts.topology = policy;
        const outcome result = load_with("undeclared_parent_link.urdf", opts);
        REQUIRE_FALSE(result.loaded);
        REQUIRE(located(result.notes, meios::diagnostic_code::undeclared_link));
    }
}
