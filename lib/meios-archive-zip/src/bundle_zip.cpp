#include "meios/archive/zip_writer.h"

#include "meios/bundle/replay.h"
#include "meios/bundle/manifest.h"
#include "meios/bundle/urdf_writer.h"
#include "meios/bundle/scanner_registry.h"

#include "meios/io/source_stack.h"

#include "meios/diagnostic/log_sink.h"

#include <map>
#include <string>
#include <memory>
#include <vector>
#include <sstream>
#include <utility>
#include <functional>

namespace meios
{

namespace
{

// bundle_impl is anon-namespace-locked in core, so its rewrite planner is
// duplicated here rather than shared: it feeds every accumulated reference into
// the shared builder, drives the transitive scanner closure, then hands back the
// original->rewritten table.
class manifest_planner : public reference_planner::planner
{
public:
    manifest_planner(manifest_builder &builder, scanner_registry &registry)
        : m_builder(builder), m_registry(registry)
    {}

    std::map<std::string, std::string> plan(const std::vector<reference_record> &refs) override
    {
        for(const reference_record &ref : refs)
            m_builder.get().add_reference(ref);
        m_builder.get().close_over(m_registry.get());
        return m_builder.get().rewrites();
    }

private:
    std::reference_wrapper<manifest_builder> m_builder;
    std::reference_wrapper<scanner_registry> m_registry;
};

template <typename Robot>
asset_manifest bundle_impl(const Robot &robot, source_stack &sources, scanner_registry &registry,
                           const zip_request &request, emit_result &out, log_sink &log)
{
    manifest_builder builder(request.bundle_name, request.collisions, sources, log);
    reference_planner hook(std::make_unique<manifest_planner>(builder, registry));
    std::ostringstream buffer;
    urdf_writer writer(buffer, log, request.bundle_name, std::move(hook));
    replay(robot, writer);

    const emit_result emitted = writer.status();
    const asset_manifest &manifest = builder.manifest();
    out = emit_result{ emitted.status, manifest.entries.size(), manifest.unresolved.size() };
    if(emitted.status == emit_status::ok)
    {
        zip_writer sink(request.archive_path, log, request.compression_level);
        out = sink.write(request.bundle_name + ".urdf", buffer.str(), manifest, request.dry_run);
    }
    if(out.status == emit_status::ok && builder.status() != emit_status::ok)
        out.status = builder.status();
    return manifest;
}

}

asset_manifest bundle_to_zip(const tree<double> &robot, source_stack &sources,
                             scanner_registry &registry, const zip_request &request,
                             emit_result &out, log_sink &log)
{
    return bundle_impl(robot, sources, registry, request, out, log);
}

asset_manifest bundle_to_zip(const model<double> &robot, source_stack &sources,
                             scanner_registry &registry, const zip_request &request,
                             emit_result &out, log_sink &log)
{
    return bundle_impl(robot, sources, registry, request, out, log);
}

}
