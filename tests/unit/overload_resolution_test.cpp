#include <meios/urdf.h>
#include <meios/model.h>
#include <meios/io.h>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

TEST_CASE("load resolves through all four call forms unambiguously", "[urdf][overload]")
{
    const std::filesystem::path path{ "no_such_model.urdf" };
    meios::load_options opts;
    meios::log_sink log;
    meios::source_stack sources;

    const meios::expected<meios::load_result, meios::load_error> a = meios::load(path);
    const meios::expected<meios::load_result, meios::load_error> b = meios::load(path, opts);
    const meios::expected<meios::load_result, meios::load_error> c = meios::load(path, opts, log);
    const meios::expected<meios::load_result, meios::load_error> d =
        meios::load(path, opts, sources, log);

    REQUIRE_FALSE(a.has_value());
    REQUIRE_FALSE(b.has_value());
    REQUIRE_FALSE(c.has_value());
    REQUIRE_FALSE(d.has_value());
}
