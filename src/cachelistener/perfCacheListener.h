#ifndef SST_CUSTOM_TRACER_PERFCACHELISTENER_H
#define SST_CUSTOM_TRACER_PERFCACHELISTENER_H

#include <sst/elements/memHierarchy/cacheListener.h>

namespace SST {
namespace MemHierarchy {

class PerfCacheListener : public CacheListener {
public:
    SST_ELI_REGISTER_SUBCOMPONENT(
        PerfCacheListener,
        "customTracer",
        "perfCacheListener",
        SST_ELI_ELEMENT_VERSION(1,0,0),
        "Cache listener for performance statistics",
        SST::MemHierarchy::CacheListener
    )

    SST_ELI_DOCUMENT_STATISTICS(
        {"read_hit",     "Number of read hits",      "count", 1},
        {"read_miss",    "Number of read misses",    "count", 1},
        {"read_na",      "Number of read NAs",       "count", 1},
        {"read_total",   "Total number of reads",    "count", 1},
        {"write_hit",    "Number of write hits",     "count", 1},
        {"write_miss",   "Number of write misses",   "count", 1},
        {"write_na",     "Number of write NAs",      "count", 1},
        {"write_total",  "Total number of writes",   "count", 1},
        {"evict_hit",    "Number of evict hits",     "count", 1},
        {"evict_miss",   "Number of evict misses",   "count", 1},
        {"evict_na",     "Number of evict NAs",      "count", 1},
        {"evict_total",  "Total number of evicts",   "count", 1},
        {"prefetch_hit", "Number of prefetch hits",  "count", 1},
        {"prefetch_miss","Number of prefetch misses","count", 1},
        {"prefetch_na",  "Number of prefetch NAs",   "count", 1},
        {"prefetch_total","Total number of prefetches","count", 1}
    )

    PerfCacheListener(ComponentId_t id, Params& params);

    void notifyAccess(const CacheListenerNotification& notify) override;

private:
    Output *out;

    Statistic<uint64_t>* stat_read_hit;
    Statistic<uint64_t>* stat_read_miss;
    Statistic<uint64_t>* stat_read_na;
    Statistic<uint64_t>* stat_read_total;

    Statistic<uint64_t>* stat_write_hit;
    Statistic<uint64_t>* stat_write_miss;
    Statistic<uint64_t>* stat_write_na;
    Statistic<uint64_t>* stat_write_total;

    Statistic<uint64_t>* stat_evict_hit;
    Statistic<uint64_t>* stat_evict_miss;
    Statistic<uint64_t>* stat_evict_na;
    Statistic<uint64_t>* stat_evict_total;

    Statistic<uint64_t>* stat_prefetch_hit;
    Statistic<uint64_t>* stat_prefetch_miss;
    Statistic<uint64_t>* stat_prefetch_na;
    Statistic<uint64_t>* stat_prefetch_total;
};

}}

#endif //SST_CUSTOM_TRACER_PERFCACHELISTENER_H
