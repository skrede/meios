#include <meios/urdf.h>
#include <meios/model.h>

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/capturing_log_sink.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <chrono>
#include <fstream>
#include <filesystem>

namespace
{

std::filesystem::path fixture(const std::string &name)
{
    return std::filesystem::path{ MEIOS_URDF_FIXTURE_DIR } / name;
}

std::filesystem::path make_pkg_root()
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    std::filesystem::path root =
        std::filesystem::temp_directory_path() / ("meios_escape_pkg_" + std::to_string(stamp));
    std::filesystem::create_directories(root / "pkg");
    return root;
}

struct diagnostic_recorder
{
    std::vector<meios::captured_diagnostic> &out;

    void operator()(meios::level, const std::string &message)
    {
        out.push_back({ meios::diagnostic_code::unspecified, meios::source_location{}, message });
    }

    void operator()(meios::level, const meios::source_location &loc, const std::string &message)
    {
        out.push_back({ meios::diagnostic_code::unspecified, loc, message });
    }

    void operator()(meios::level, meios::diagnostic_code code, const meios::source_location &loc,
                    const std::string &message)
    {
        out.push_back({ code, loc, message });
    }
};

}

TEST_CASE("a duplicate attribute propagates through load()'s return under the silent default",
          "[urdf][load_failure]")
{
    meios::load_options opts;
    const meios::expected<meios::model<double>, meios::load_error> result =
        meios::load(fixture("dup_attr.urdf"), opts);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == meios::diagnostic_code::duplicate_attribute);
}

TEST_CASE("trailing content propagates through load()'s return under the silent default",
          "[urdf][load_failure]")
{
    meios::load_options opts;
    const meios::expected<meios::model<double>, meios::load_error> result =
        meios::load(fixture("trailing_garbage.urdf"), opts);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == meios::diagnostic_code::trailing_content);
}

TEST_CASE("an undefined property under eval_policy::fail returns a located error", "[urdf][load_failure]")
{
    meios::load_options opts;
    opts.eval = meios::eval_policy::fail;
    const meios::expected<meios::model<double>, meios::load_error> result =
        meios::load(fixture("undefined_property.xacro"), opts);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == meios::diagnostic_code::undefined_property);
    REQUIRE(result.error().loc.line > 0);
    REQUIRE(result.error().loc.column > 0);
}

TEST_CASE("an unresolvable $(find) in an attribute returns a located unresolved_find error",
          "[urdf][load_failure]")
{
    meios::load_options opts;
    opts.eval = meios::eval_policy::fail;
    const meios::expected<meios::model<double>, meios::load_error> result =
        meios::load(fixture("unresolvable_find_attr.xacro"), opts);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == meios::diagnostic_code::unresolved_find);
    REQUIRE(result.error().loc.line > 0);
    REQUIRE(result.error().loc.column > 0);
}

TEST_CASE("an unresolvable $(find) include under eval_policy::fail returns a located error",
          "[urdf][load_failure]")
{
    meios::load_options opts;
    opts.eval = meios::eval_policy::fail;
    const meios::expected<meios::model<double>, meios::load_error> result =
        meios::load(fixture("unresolvable_find_include.xacro"), opts);

    // The include seam extracts $(find ...) textually and reports the containing
    // include's failure to resolve the composed path, not the bare cmd_find code.
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == meios::diagnostic_code::unresolved_include);
    REQUIRE(result.error().loc.line > 0);
    REQUIRE(result.error().loc.column > 0);
}

TEST_CASE("the undefined-property error reports the failing token column, not the node anchor",
          "[urdf][load_failure]")
{
    meios::load_options opts;
    opts.eval = meios::eval_policy::fail;
    const meios::expected<meios::model<double>, meios::load_error> result =
        meios::load(fixture("undefined_property.xacro"), opts);

    // On line 3 `  <link name="${undeclared_link_name}"/>` the failing `${` starts at
    // 1-based column 15; the coarse node anchor would be column 4 (the element name).
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().loc.line == 3);
    REQUIRE(result.error().loc.column == 15);
}

TEST_CASE("an entity-before-token value degrades to a safe column, never a wrong offset",
          "[urdf][load_failure]")
{
    meios::load_options opts;
    opts.eval = meios::eval_policy::fail;
    const meios::expected<meios::model<double>, meios::load_error> result =
        meios::load(fixture("entity_before_token.xacro"), opts);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == meios::diagnostic_code::undefined_property);
    REQUIRE(result.error().loc.line > 0);
    REQUIRE(result.error().loc.column > 0);

    // On line 3 `  <link name="pre&amp;${undeclared}"/>` the decode-replay maps the
    // failing token to raw column 23; a degrade may fall back to the value-start
    // column (15) or the node anchor (4). No other column is defensible — a value
    // outside this set would be a confidently-wrong offset.
    const int column = result.error().loc.column;
    const bool safe = column == 23 || column == 15 || column == 4;
    REQUIRE(safe);
}

TEST_CASE("eval_policy::warn keeps the specific evaluator code on a genuine error",
          "[urdf][load_failure]")
{
    meios::load_options opts;
    opts.eval = meios::eval_policy::warn;
    const meios::expected<meios::model<double>, meios::load_error> result =
        meios::load(fixture("undefined_property.xacro"), opts);

    // An undefined name is a genuine evaluator error, not an unsupported construct,
    // so leniency does not leave it verbatim: the load still fails and the specific
    // code must survive the boundary replay rather than degrade to unspecified.
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == meios::diagnostic_code::undefined_property);
}

TEST_CASE("a failed xacro load records no xml_parse_error cascade after the primary",
          "[urdf][load_failure]")
{
    std::vector<meios::captured_diagnostic> records;
    meios::log_sink_f sink{ diagnostic_recorder{ records } };

    meios::load_options opts;
    opts.eval = meios::eval_policy::fail;
    const meios::expected<meios::model<double>, meios::load_error> result =
        meios::load(fixture("undefined_property.xacro"), opts, sink);

    REQUIRE_FALSE(result.has_value());

    bool primary_present = false;
    int xml_parse_records = 0;
    for(const meios::captured_diagnostic &record : records)
    {
        if(record.code == meios::diagnostic_code::undefined_property)
            primary_present = true;
        if(record.code == meios::diagnostic_code::xml_parse_error)
            ++xml_parse_records;
    }
    REQUIRE(primary_present);
    REQUIRE(xml_parse_records == 0);
}

TEST_CASE("an undefined material under material_policy::fail propagates the specific code",
          "[urdf][load_failure]")
{
    meios::load_options opts;
    opts.materials = meios::material_policy::fail;
    const meios::expected<meios::model<double>, meios::load_error> result =
        meios::load(fixture("unresolved_material.urdf"), opts);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == meios::diagnostic_code::undefined_material);
}

TEST_CASE("an unresolved mesh under missing_asset::fail propagates the specific code",
          "[urdf][load_failure]")
{
    meios::load_options opts;
    opts.on_missing = meios::missing_asset::fail;
    const meios::expected<meios::model<double>, meios::load_error> result =
        meios::load(fixture("package_mesh.urdf"), opts);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == meios::diagnostic_code::unresolved_mesh);
}

TEST_CASE("a warn material policy still returns a populated model", "[urdf][load_failure]")
{
    meios::load_options opts;
    opts.materials = meios::material_policy::warn;
    const meios::expected<meios::model<double>, meios::load_error> result =
        meios::load(fixture("unresolved_material.urdf"), opts);

    REQUIRE(result.has_value());
}

TEST_CASE("a warn missing-asset policy still returns a populated model", "[urdf][load_failure]")
{
    meios::load_options opts;
    opts.on_missing = meios::missing_asset::warn;
    const meios::expected<meios::model<double>, meios::load_error> result =
        meios::load(fixture("package_mesh.urdf"), opts);

    REQUIRE(result.has_value());
}

TEST_CASE("an over-long include path becomes a typed error, never a filesystem_error abort",
          "[urdf][load_failure]")
{
    // A single include component past the OS name limit makes the throwing
    // filesystem overloads raise ENAMETOOLONG; the load must fail closed with a
    // typed diagnostic instead of terminating the process.
    meios::load_options opts;
    opts.eval = meios::eval_policy::fail;
    meios::expected<meios::model<double>, meios::load_error> result =
        meios::load(fixture("overlong_include.xacro"), opts);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == meios::diagnostic_code::unresolved_include);
}

TEST_CASE("a root-escaping package path fails the load under the default policy",
          "[urdf][load_failure]")
{
    const std::filesystem::path root = make_pkg_root();

    meios::load_options opts;
    opts.package_roots = { root };
    const meios::expected<meios::model<double>, meios::load_error> result =
        meios::load(fixture("package_escape_mesh.urdf"), opts);

    std::filesystem::remove_all(root);

    REQUIRE_FALSE(result.has_value());
}
