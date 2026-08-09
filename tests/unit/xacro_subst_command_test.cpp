#include "xacro_subst_fixture.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("substitution command dispatch resolves find, arg, eval and dirname",
          "[xacro][subst][cmd]")
{
    std::vector<std::pair<meios::level, std::string>> records;
    meios::log_sink_f log{ captured_log{ records } };

    std::filesystem::path root =
        std::filesystem::temp_directory_path() / "meios_subst_fixture";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "pkg");

    meios::directory_source on_disk{ root, log };
    meios::memory_source in_memory{ log };
    in_memory.add("bytespkg", "meshes/arm.dae", "resolved-from-bytes");
    meios::source_stack sources{ std::move(on_disk), std::move(in_memory) };

    meios::eval_scope scope;
    scope.set("width", *meios::value::make_real(0.3));
    std::filesystem::path document = root / "sub" / "robot.xacro";

    SECTION("$(find) resolves a package directory to a filesystem path")
    {
        const substitution_result out =
            meios::substitute("$(find pkg)/x.stl", scope, sources, document, log);
        REQUIRE(out.has_value());
        REQUIRE(out->text == std::filesystem::weakly_canonical(root / "pkg").string() + "/x.stl");
    }

    SECTION("$(find ${pkg}) resolves the inner expression before dispatch")
    {
        scope.set("pkgname", meios::value{ std::string("pkg") });
        const substitution_result out =
            meios::substitute("$(find ${pkgname})/x.stl", scope, sources, document, log);
        REQUIRE(out.has_value());
        REQUIRE(out->text == std::filesystem::weakly_canonical(root / "pkg").string() + "/x.stl");
    }

    SECTION("$(find) on a byte-backed layer loud-fails rather than naming a directory")
    {
        const substitution_result out =
            meios::substitute("$(find bytespkg)", scope, sources, document, log);
        REQUIRE_FALSE(out.has_value());
        REQUIRE(out.error().code == meios::diagnostic_code::unresolved_find);
        REQUIRE(locates_a_file(out));
        REQUIRE(any_contains(records, "did not resolve"));
    }

    SECTION("$(find) reaches past a byte-backed layer to the layer that holds the package")
    {
        std::filesystem::create_directories(root / "shadowed" / "meshes");
        std::ofstream(root / "shadowed" / "meshes" / "base.stl") << "solid\n";

        meios::memory_source overlay{ log };
        overlay.add("shadowed", "meshes/override.stl", "override-bytes");
        meios::directory_source holder{ root, log };
        meios::source_stack stacked{ std::move(overlay), std::move(holder) };

        const substitution_result out =
            meios::substitute("$(find shadowed)/meshes/base.stl", scope, stacked, document, log);
        REQUIRE(out.has_value());
        REQUIRE(std::filesystem::exists(out->text));
    }

    SECTION("an unresolved $(find) loud-fails")
    {
        const substitution_result out =
            meios::substitute("$(find missing)", scope, sources, document, log);
        REQUIRE_FALSE(out.has_value());
        REQUIRE(out.error().code == meios::diagnostic_code::unresolved_find);
        REQUIRE(locates_a_file(out));
        REQUIRE(any_contains(records, "did not resolve"));
    }

    SECTION("$(arg) returns the bound value")
    {
        const substitution_result out =
            meios::substitute("$(arg width)", scope, sources, document, log);
        REQUIRE(out.has_value());
        REQUIRE(out->text == "0.3");
    }

    SECTION("$(arg name default) falls back to the default when unset")
    {
        const substitution_result out =
            meios::substitute("$(arg height 0.5)", scope, sources, document, log);
        REQUIRE(out.has_value());
        REQUIRE(out->text == "0.5");
    }

    SECTION("$(arg name default) with a bound name never evaluates an unresolvable default")
    {
        scope.set("mesh_pkg", meios::value{ std::string("pkg") });
        const substitution_result out =
            meios::substitute("$(arg mesh_pkg $(find absent_pkg))", scope, sources, document, log);
        REQUIRE(out.has_value());
        REQUIRE(out->text == "pkg");
        REQUIRE_FALSE(any_contains(records, "did not resolve"));
    }

    SECTION("an unset $(arg) with no default loud-fails")
    {
        const substitution_result out =
            meios::substitute("$(arg height)", scope, sources, document, log);
        REQUIRE_FALSE(out.has_value());
        REQUIRE(out.error().code == meios::diagnostic_code::unresolved_arg);
        REQUIRE(locates_a_file(out));
        REQUIRE(any_contains(records, "unset and has no default"));
    }

    SECTION("a nested default whose inner locate fails carries the inner refusal outward")
    {
        const substitution_result out = meios::substitute("$(arg height $(find absent))", scope,
                                                          sources, document, log);
        REQUIRE_FALSE(out.has_value());
        REQUIRE(out.error().code == meios::diagnostic_code::unresolved_find);
        REQUIRE(locates_a_file(out));
    }

    SECTION("$(eval) routes the remainder to the expression evaluator")
    {
        const substitution_result out =
            meios::substitute("$(eval 1 + 2)", scope, sources, document, log);
        REQUIRE(out.has_value());
        REQUIRE(out->text == "3");
    }

    SECTION("$(dirname) resolves the current document directory")
    {
        const substitution_result out =
            meios::substitute("$(dirname)/mesh", scope, sources, document, log);
        REQUIRE(out.has_value());
        REQUIRE(out->text == document.parent_path().string() + "/mesh");
    }

    SECTION("an unknown command loud-fails")
    {
        const substitution_result out =
            meios::substitute("$(bogus x)", scope, sources, document, log);
        REQUIRE_FALSE(out.has_value());
        REQUIRE(out.error().code == meios::diagnostic_code::unknown_substitution);
        REQUIRE(locates_a_file(out));
        REQUIRE(any_contains(records, "unknown substitution command"));
    }

    std::filesystem::remove_all(root);
}

TEST_CASE("an input carrying two refusals reports exactly one error-level record",
          "[xacro][subst][cmd]")
{
    std::vector<std::pair<meios::level, std::string>> records;
    meios::log_sink_f log{ captured_log{ records } };
    meios::source_stack sources;
    meios::eval_scope scope;
    std::filesystem::path document{ "robot.xacro" };

    const substitution_result out =
        meios::substitute("$(bogus a) and $(other b)", scope, sources, document, log);

    REQUIRE_FALSE(out.has_value());
    REQUIRE(records_at(records, meios::level::error) == 1);
}
