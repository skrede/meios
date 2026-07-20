# Engine guide: `model_sink` → your own types

The engine tier skips the intermediate `model`. Instead of receiving a container and copying out of
it, you declare a type that satisfies the `model_sink` concept and meios drives it directly: one
`on_robot` call, then an `on_material` per material, an `on_link` per link, an `on_joint` per joint,
and a final `finish()`. The resolved description lands in your own representation — a scene graph, a
solver's body list, a renderer's node table — as it is read, with nothing held on meios's side.

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

    scene.on_robot(meios::robot_info{});
    scene.finish();

    meios::log_sink &channel = capture;
    channel.log(meios::level::warn, meios::diagnostic_code::unresolved_mesh,
                meios::source_location{"robot.urdf", 12, 4}, "mesh left unresolved");

    std::cout << "sink '" << scene.name << "' collected " << scene.bodies.size() << " bodies and "
              << journal.size() << " diagnostics\n";
    if (!journal.empty())
        std::cout << "first diagnostic code: "
                  << meios::to_string(journal.front().code) << '\n';
}
```

## Why this tier exists

If you were going to copy every link and joint out of a `model` into your own type anyway, the
consumer tier makes you allocate the `model` first and walk it second. The engine tier removes that
round trip: your `on_link` is the copy, and there is no `model` to free afterward. The typed
`diagnostic_code` on the sink callable is the same enum `load_error::code` carries, so a program that
records diagnostics can branch on the code — an unresolved mesh, an LFS-pointer mesh, an invalid
topology — without matching against message strings that are free to change.

For the plain `load()` → `model` path, see the [consumer guide](consumer-guide.md).
