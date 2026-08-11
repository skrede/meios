#ifndef HPP_GUARD_MEIOS_TESTS_DIFFERENTIAL_VERDICT_H
#define HPP_GUARD_MEIOS_TESTS_DIFFERENTIAL_VERDICT_H

#include "unit/oracle_records.h"

#include <string>
#include <vector>
#include <optional>
#include <tuple>

namespace differential
{

// The first inventory id absent from seen_ids, in inventory order. A missing verdict fails the
// run by naming which comparison the harness owes and never produced, before any pass/fail count
// is reported -- a selector that quietly matched nothing must not read as coverage.
inline std::optional<std::string> first_missing_verdict(const std::vector<oracle::row> &inventory,
                                                         const std::vector<std::string> &seen_ids)
{
    for(const oracle::row &entry : inventory)
    {
        if(entry.fields.empty())
            continue;
        const std::string &id = entry.fields.front();
        bool found = false;
        for(const std::string &seen : seen_ids)
        {
            if(seen == id)
            {
                found = true;
                break;
            }
        }
        if(!found)
            return id;
    }
    return std::nullopt;
}

// A divergence absent from the manifest under id, or present with recorded upstream/meios text
// differing from what was actually observed, is not a reviewed divergence -- the exact-text
// comparison is what stops a known id from masking a changed, unrelated disagreement.
inline bool matches_manifest(const std::vector<oracle::row> &manifest, const std::string &id,
                             const std::string &observed_upstream,
                             const std::string &observed_meios, std::string &detail)
{
    for(const oracle::row &entry : manifest)
    {
        if(entry.fields.size() < 3 || entry.fields[0] != id)
            continue;
        if(entry.fields[1] == observed_upstream && entry.fields[2] == observed_meios)
            return true;
        detail = "divergence manifest entry '" + id + "' recorded upstream='" + entry.fields[1]
               + "' meios='" + entry.fields[2] + "', but this run observed upstream='"
               + observed_upstream + "' meios='" + observed_meios + "'";
        return false;
    }
    detail = "divergence '" + id + "' (upstream='" + observed_upstream + "', meios='"
           + observed_meios + "') is not listed in the divergence manifest";
    return false;
}

// Every manifest id whose exact recorded (id, upstream, meios) triple was not among the
// divergences this run actually observed. A listed entry that stopped reproducing is a
// suppression, not a reviewed divergence, and must fail exactly as an unlisted one does.
inline std::vector<std::string> stale_manifest_entries(
    const std::vector<oracle::row> &manifest,
    const std::vector<std::tuple<std::string, std::string, std::string>> &observed)
{
    std::vector<std::string> stale;
    for(const oracle::row &entry : manifest)
    {
        if(entry.fields.size() < 3)
            continue;
        const std::tuple<std::string, std::string, std::string> triple{ entry.fields[0],
                                                                         entry.fields[1],
                                                                         entry.fields[2] };
        bool found = false;
        for(const std::tuple<std::string, std::string, std::string> &one : observed)
        {
            if(one == triple)
            {
                found = true;
                break;
            }
        }
        if(!found)
            stale.push_back(entry.fields[0]);
    }
    return stale;
}

}

#endif
