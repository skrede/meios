#include "uri_fixture.h"

#include <meios/urdf/asset_uri.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>

#include <catch2/catch_test_macros.hpp>

#include <vector>
#include <filesystem>
#include <string_view>

// A drive-letter spelling classifies identically on all three platforms even though the load
// verdict for it legitimately does not, which is why the claim is asserted against the
// classifier here rather than through a row.
TEST_CASE("uri classification", "[urdf][uri]")
{
    using meios::detail::asset_uri_form;
    for(std::string_view drive : {"C:/meshes/arm.dae", "C:\\meshes\\arm.dae"})
    {
        INFO(drive);
        CHECK_FALSE(meios::detail::scheme_of(drive).has_value());
        CHECK(meios::detail::classify_asset_uri(drive) != asset_uri_form::foreign_scheme);
    }
    CHECK(meios::detail::classify_asset_uri("package://pkg/meshes/base.stl") == asset_uri_form::package);
    CHECK(meios::detail::classify_asset_uri("file:///meshes/base.stl") == asset_uri_form::absolute);
    CHECK(meios::detail::classify_asset_uri("http://example.invalid/base.stl") == asset_uri_form::foreign_scheme);
    CHECK(meios::detail::classify_asset_uri("model://pkg/base.stl") == asset_uri_form::foreign_scheme);
    CHECK_FALSE(meios::detail::scheme_of("3d://meshes/base.stl").has_value());
    CHECK(meios::detail::classify_asset_uri("3d://meshes/base.stl") == asset_uri_form::relative);

    for(std::string_view spelling : {"file:///c:/arm/base.stl", "file://c:/arm/base.stl"})
    {
        INFO(spelling);
        CHECK(meios::detail::file_uri_to_path(spelling) == "c:/arm/base.stl");
    }
    CHECK(meios::detail::file_uri_to_path(R"(file:///c:\arm\base.stl)") == R"(c:\arm\base.stl)");
    CHECK(meios::detail::file_uri_to_path("file:///opt/arm/base.stl") == "/opt/arm/base.stl");
    CHECK(meios::detail::file_uri_to_path("file:///x:data/mesh.stl") == "/x:data/mesh.stl");
    CHECK(meios::detail::classify_asset_uri(meios::detail::file_uri_to_path("file:///x:data/mesh.stl")) == meios::detail::classify_asset_uri("/x:data/mesh.stl"));
}

// The lenient drive spelling is a published accepted form whose load verdict differs by
// platform, so like the bare drive spellings above it takes a case rather than a row. The
// non-empty half of the first claim is load-bearing: an authority helper answering empty for
// everything would satisfy the empty half and refuse nothing.
TEST_CASE("uri file authority", "[urdf][uri]")
{
    CHECK(meios::detail::file_authority("file://c:/arm/meshes/base.stl").empty());
    CHECK(meios::detail::file_authority("file:///opt/arm/meshes/base.stl").empty());
    CHECK(meios::detail::file_authority("file://host/share/base.stl") == "host");
    CHECK(meios::detail::file_authority("file://localhost/opt/arm/base.stl") == "localhost");
    CHECK(meios::detail::file_authority("file://cd/x") == "cd");
    CHECK(meios::detail::file_authority("file://C|/x") == "C|");

    const uri::sandbox tree;
    const std::filesystem::path document = tree.write_document(R"(<?xml version="1.0"?>)"
                                                               R"(<robot name="drive"><link name="base_link"><visual><geometry>)"
                                                               R"(<mesh filename="file://c:/arm/meshes/base.stl"/>)"
                                                               R"(</geometry></visual></link></robot>)");
    meios::load_options opts;
    opts.package_roots.push_back(tree.root());
    std::vector<uri::captured> raised;
    meios::log_sink_f capture{uri::recorder{raised}};
    const meios::expected<meios::load_result, meios::load_error> loaded = meios::load(document, opts, capture);
    const bool drive_absolute                                           = std::filesystem::path("c:/arm/meshes/base.stl").is_absolute();
    CHECK_FALSE(uri::located(raised, meios::level::error, "unsupported_uri_authority"));
    CHECK(uri::located(raised, meios::level::error, "malformed_asset_uri") == !drive_absolute);
    CHECK_FALSE(static_cast<bool>(loaded));
}

// Every table row names its document absolutely and registers its directory as a package
// root, so no row can catch a base that is empty because the caller named the document by a
// bare filename. No root is registered here on purpose: the document's own directory is then
// the only thing that can contain the asset, which is exactly the claim under test.
TEST_CASE("uri relative base survives a document named relatively", "[urdf][uri]")
{
    const uri::sandbox tree;
    const std::filesystem::path document = tree.write_document(uri::fixture_text("relative_resolves.urdf"));
    const std::filesystem::path previous = std::filesystem::current_path();
    std::filesystem::current_path(document.parent_path());
    const meios::expected<meios::load_result, meios::load_error> loaded = meios::load(document.filename(), meios::load_options{});
    std::filesystem::current_path(previous);
    REQUIRE(static_cast<bool>(loaded));
    CHECK(uri::resolved_mesh_path(loaded->robot).has_value());
}
