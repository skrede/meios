#include "urdf_yaml_flatten_fixture.h"

#include "meios/urdf/yaml_resource.h"

#include "../marker_spelling.h"

#include <meios/eval/python_evaluator.h>

#include <meios/urdf.h>
#include <meios/model.h>
#include <meios/xacro.h>

#include <meios/io/source_stack.h>
#include <meios/io/memory_source.h>
#include <meios/io/source_handle.h>

#include <meios/diagnostic/expansion_error.h>
#include <meios/diagnostic/capturing_log_sink.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <memory>
#include <string>
#include <vector>
#include <fstream>
#include <utility>
#include <iterator>
#include <optional>
#include <filesystem>
#include <functional>

namespace
{

struct tally
{
    int &errors;

    void operator()(meios::level lvl, const std::string &)
    {
        if(lvl == meios::level::error)
            ++errors;
    }

    void operator()(meios::level lvl, const meios::source_location &, const std::string &)
    {
        if(lvl == meios::level::error)
            ++errors;
    }
};

attempt flatten_over(const std::filesystem::path &path, meios::source_stack &sources)
{
    journal book;
    meios::log_sink_f sink{std::ref(book)};
    meios::capturing_log_sink capture{sink};
    const meios::load_options opts                                = python_options();
    meios::expected<meios::load_result, meios::load_error> loaded = meios::load(path, opts, sources, capture);
    if(!loaded)
        return {book.errors, book.report + loaded.error().message, std::nullopt};
    return {book.errors, std::move(book.report), std::move(loaded->robot)};
}

struct flattened
{
    int errors;
    std::optional<std::string> document;
};

std::string bytes_of(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

// A loaded model carries parsed numbers, not the text they were emitted as, so it cannot
// answer whether a marker reached the document. This runs the expansion the load path runs,
// stopping one stage earlier, so the emitted bytes themselves are what a case asserts on.
flattened flatten_to_text(const std::filesystem::path &path)
{
    journal book;
    meios::log_sink_f sink{std::ref(book)};
    meios::source_stack sources;
    const std::vector<std::filesystem::path> roots = fixture_roots();
    meios::eval_scope scope;
    scope.install_text_loader(meios::detail::make_yaml_text_loader(sources, roots, sink));
    const std::string source = bytes_of(path);
    const meios::expected<meios::expansion, meios::expansion_error> out =
            meios::expand(source, scope, sources, path, meios::expansion_limits{}, meios::eval_policy::fail, std::make_shared<meios::evaluator_handle>(meios::python_evaluator{}), sink);
    if(!out)
        return {book.errors, std::nullopt};
    return {book.errors, out->document};
}

}

TEST_CASE("a yaml-driven arm flattens end-to-end through the python backend", "[urdf][yaml][flatten]")
{
    int errors = 0;
    meios::log_sink_f sink{tally{errors}};

    const meios::load_options opts = python_options();

    const meios::expected<meios::load_result, meios::load_error> loaded = meios::load(fixture("yaml_arm/arm.urdf.xacro"), opts, sink);

    REQUIRE(loaded.has_value());
    const meios::model<double> &robot = loaded->robot;
    REQUIRE(errors == 0);
    REQUIRE(robot.links.size() == 3);

    const meios::joint<double> *shoulder = find_joint(robot, "shoulder_joint");
    REQUIRE(shoulder != nullptr);
    REQUIRE(shoulder->origin.translation.z == Catch::Approx(0.25));

    REQUIRE(shoulder->limits.has_value());
    REQUIRE(shoulder->limits->lower == Catch::Approx(-std::acos(-1.0) / 2.0));
    REQUIRE(shoulder->limits->upper == Catch::Approx(1.5));
    REQUIRE(shoulder->limits->effort == Catch::Approx(100.0));
    REQUIRE(shoulder->limits->velocity == Catch::Approx(2.0));

    const meios::joint<double> *elbow = find_joint(robot, "elbow_joint");
    REQUIRE(elbow != nullptr);
    REQUIRE(elbow->origin.translation.x == Catch::Approx(0.27));
    REQUIRE(elbow->origin.translation.z == Catch::Approx(0.4));

    REQUIRE(elbow->limits.has_value());
    REQUIRE(elbow->limits->lower == Catch::Approx(-2.0));
    REQUIRE(elbow->limits->upper == Catch::Approx(2.5));
    REQUIRE(elbow->limits->effort == Catch::Approx(60.0));
    REQUIRE(elbow->limits->velocity == Catch::Approx(3.5));
}

TEST_CASE("the flattened arm carries the dotted reads", "[urdf][yaml][flatten]")
{
    const flattened flat = flatten_to_text(fixture("yaml_arm/arm.urdf.xacro"));

    REQUIRE(flat.document.has_value());
    REQUIRE(flat.errors == 0);
    REQUIRE(flat.document->find("xyz=\"0.27 0 0.4\"") != std::string::npos);
    REQUIRE(flat.document->find("lower=\"-2.0\" upper=\"2.5\"") != std::string::npos);
}

// A robot description writes only scalar leaves, so no container is ever emitted whole from one
// and an absence assertion over its flattened text cannot fail. The probe document exists to
// carry a container across each seam the strip guards, one marker at each: a yaml-sourced
// mapping into an attribute value, a plain list into element text. The two span assertions match
// each container's body without the delimiters around it, so an unstripped marker leaves them
// passing and the absence check is what reports it.
TEST_CASE("a description emitting containers whole carries no container marker", "[urdf][yaml][flatten]")
{
    const flattened flat = flatten_to_text(fixture("container_probe/probe.xacro"));

    REQUIRE(flat.document.has_value());
    REQUIRE(flat.errors == 0);
    REQUIRE(flat.document->find("{'upper': 1.5}") != std::string::npos);
    REQUIRE(flat.document->find("[1, 2]") != std::string::npos);
    marker::absent_from(*flat.document);
}

TEST_CASE("an in-memory source layer serves a working configuration", "[urdf][yaml][flatten]")
{
    meios::log_sink silent;
    meios::memory_source layer{silent};
    layer.add("memory_pkg", "config.yaml",
              "joints:\n  shoulder:\n    height: 0.5\n"
              "    lower: !degrees -30\n"
              "    upper: 3.0\n"
              "    effort: 50.0\n"
              "    velocity: 1.0\n");

    meios::source_stack sources;
    sources.push_back(meios::source_handle(std::move(layer)));
    expect_shoulder(flatten_over(fixture("yaml_pkg/bytes.urdf.xacro"), sources), 0.5, 3.0);
}
