# Engine guide: `model_sink` → your own types

The engine tier gives up the intermediate `model`. Instead of receiving a container and copying out
of it, you declare a type that satisfies the `model_sink` concept and meios drives it directly: one
`on_robot` call, then an `on_material` per material, an `on_link` per link, an `on_joint` per joint,
and a final `finish()`. The resolved description lands in your own representation — a scene graph, a
solver's body list, a renderer's node table — with no `model` left over for you to free.

There are two entry points and the same sink serves both. `basic_parser<urdf_reader>::parse` drives
a sink straight from a buffer you already hold. `load_into` takes a path and runs the whole pipeline
behind it — file, xacro expansion, `package://` and `$(find)` resolution, diagnostics — then hands
your sink the result. `load_into` is the one you want unless you are already holding bytes.

## Satisfying the concept

`model_sink` is a compile-time concept, not a base class to inherit. Any type with the five member
functions below satisfies it; the `static_assert` makes a wrong signature a build error rather than a
silent no-match. The records handed to each method — `robot_info`, `material`, `link`, `joint` — are
the resolved values, so your sink stores exactly the fields it cares about.

Alongside the model sink, a `log_sink_f` adapts a callable into the diagnostic channel. The channel
carries three call shapes — unlocated, located, and the typed four-argument form that also delivers
the `diagnostic_code` — so the callable is a small struct with one `operator()` per shape rather than
a single-shape lambda. The four-argument overload records the typed code straight into your own record
type, no string parsing.

<!-- meios:snippet name=custom-sink tu -->
```cpp
#include <meios/urdf.h>

#include <meios/sink/model_sink.h>

#include <meios/diagnostic/log_sink.h>

#include <string>
#include <vector>
#include <iostream>

struct scene_builder
{
    void on_robot(const meios::robot_info &robot) { name = robot.name; }
    void on_material(const meios::material<double> &) {}
    void on_link(const meios::link<double> &node) { bodies.push_back(node.name); }
    void on_joint(const meios::joint<double> &edge) { articulations.push_back(edge.name); }
    void finish() { built = true; }

    std::string name;
    std::vector<std::string> bodies;
    std::vector<std::string> articulations;
    bool built = false;
};

static_assert(meios::model_sink<scene_builder>);

struct diagnostic_record
{
    meios::level severity;
    meios::diagnostic_code code;
    std::string message;
};

struct diagnostic_collector
{
    std::vector<diagnostic_record> &journal;

    void operator()(meios::level lvl, const std::string &message)
    {
        journal.push_back(diagnostic_record{lvl, meios::diagnostic_code::unspecified, message});
    }

    void operator()(meios::level lvl, const meios::source_location &, const std::string &message)
    {
        journal.push_back(diagnostic_record{lvl, meios::diagnostic_code::unspecified, message});
    }

    void operator()(meios::level lvl, meios::diagnostic_code code, const meios::source_location &,
                    const std::string &message)
    {
        journal.push_back(diagnostic_record{lvl, code, message});
    }
};

int main()
{
    scene_builder scene;
    std::vector<diagnostic_record> journal;
    meios::log_sink_f capture{diagnostic_collector{journal}};

    meios::load_options opts;
    const meios::load_summary summary = meios::load_into("robot.urdf", opts, scene, capture);

    if (!scene.built)
    {
        std::cerr << "the load failed and the scene was left untouched\n";
        for (const meios::captured_diagnostic &record : summary.diagnostics)
            std::cerr << record.loc.file.string() << ':' << record.loc.line << ": "
                      << meios::to_string(record.code) << ": " << record.message << '\n';
        return 1;
    }

    std::cout << "sink '" << scene.name << "' collected " << scene.bodies.size() << " bodies and "
              << summary.diagnostics.size() << " diagnostics\n";
    if (!has(summary.claims, meios::completeness::deployment_complete))
        std::cout << "at least one asset did not resolve\n";
}
```

The `log_sink` argument is optional: `load_into(path, opts, scene)` runs the same pipeline and only
withholds the live diagnostic stream, because the summary carries every diagnostic anyway.

## Your sink is untouched unless the load succeeded

`load_into` builds the model internally first and drives your sink only once that has succeeded. A
failed load therefore leaves your types exactly as they were — not partially filled, not filled and
then rolled back — so there is no rollback for you to implement and no half-built state to detect.
The `built` flag above is a sufficient test: if it is false, nothing else on the sink moved either.

The cost of that guarantee, and it is a real one: your types are populated from a staged model rather
than streamed as the document is read, so peak memory holds the staged model and your own copy at
once. The staged model is built for the plain `load()` path regardless, so the increment over that
path is your own representation. If you cannot afford it, drive `parse` from a buffer instead — it
streams, and it is the same sink either way.

One record does not survive staging: `robot_info` reaches your sink carrying only the robot's name,
because the staged model does not keep the `<robot>` element's extensions or its version attribute.
`parse` hands you the full record.

## Diagnostics arrive through the summary, not through the sink

There is no `on_diagnostic` hook on `model_sink`, and there will not be one. The concept describes
what a sink receives of the *model*; diagnostics reach you two other ways, and adding a third would
only duplicate them. Take the returned `load_summary` for the accumulated list after the fact, or
pass a `log_sink` for the same records as they are produced. `load_summary` spells its two members
exactly as `load_result` does, so moving between `load()` and `load_into` teaches you nothing new.

`summary.claims` answers what the load established rather than what it logged: whether the document
parsed, whether the topology holds, whether every referenced asset resolved. Branch on it with
`has()` rather than counting warnings.

## A sink written against `parse` needs no edit

The concept is the whole contract, and `load_into` is constrained on that same concept, unchanged.
A sink you already drive from a buffer works against a path with no edit — the compile-time proof
lives in `tests/compile/pass/sink_serves_both_entry_points.cpp`, which puts one sink type through
both entry points and fails to build if either drifts.

The runtime proof is `tests/integration/consumer/facade.cpp`, a translation unit in the standalone
project under `tests/integration/consumer/`. That project finds meios through `find_package` against
an *installed* tree rather than the build tree, defines its sink entirely outside meios's sources,
and asserts both halves of the guarantee: the counts after a successful load, and every counter at
zero after a failed one. Three continuous-integration jobs — Linux, macOS and Windows — configure,
build and run it, so a change that breaks the facade breaks a build rather than a reader.

## Why this tier exists

If you were going to copy every link and joint out of a `model` into your own type anyway, the
consumer tier makes you allocate the `model` first and walk it second; the engine tier makes your
`on_link` the copy and leaves you nothing to free. The typed `diagnostic_code` is the same enum
`load_error::code` carries, so a program that records diagnostics can branch on the code — an
unresolved mesh, an LFS-pointer mesh, an invalid topology — without matching against message strings
that are free to change.

For the plain `load()` → `model` path, see the [consumer guide](consumer-guide.md).
