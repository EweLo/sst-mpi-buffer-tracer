#include "perfCacheListener.h"

namespace SST {
namespace MemHierarchy {

PerfCacheListener::PerfCacheListener(ComponentId_t id, Params &params) : CacheListener(id, params) {
    stat_read_hit    = registerStatistic<uint64_t>("read_hit");
    stat_read_miss   = registerStatistic<uint64_t>("read_miss");
    stat_read_na     = registerStatistic<uint64_t>("read_na");
    stat_read_total  = registerStatistic<uint64_t>("read_total");

    stat_write_hit   = registerStatistic<uint64_t>("write_hit");
    stat_write_miss  = registerStatistic<uint64_t>("write_miss");
    stat_write_na    = registerStatistic<uint64_t>("write_na");
    stat_write_total = registerStatistic<uint64_t>("write_total");

    stat_evict_hit   = registerStatistic<uint64_t>("evict_hit");
    stat_evict_miss  = registerStatistic<uint64_t>("evict_miss");
    stat_evict_na    = registerStatistic<uint64_t>("evict_na");
    stat_evict_total = registerStatistic<uint64_t>("evict_total");

    stat_prefetch_hit    = registerStatistic<uint64_t>("prefetch_hit");
    stat_prefetch_miss   = registerStatistic<uint64_t>("prefetch_miss");
    stat_prefetch_na     = registerStatistic<uint64_t>("prefetch_na");
    stat_prefetch_total  = registerStatistic<uint64_t>("prefetch_total");

    out = new Output("PerfCacheListener[@f:@l:@p] ", 1, 0, Output::STDOUT);

    out->verbose(CALL_INFO, 1, 0, "PerfCacheListener was constructed for Cache '%s'.\n", getParentComponentName().c_str());
}

void PerfCacheListener::notifyAccess(const CacheListenerNotification& notify) {
    NotifyAccessType accessType = notify.getAccessType();
    NotifyResultType resultType = notify.getResultType();

    switch (accessType) {
        case READ:
            stat_read_total->addData(1);
            if (resultType == HIT) stat_read_hit->addData(1);
            else if (resultType == MISS) stat_read_miss->addData(1);
            else if (resultType == NA) stat_read_na->addData(1);
            break;
        case WRITE:
            stat_write_total->addData(1);
            if (resultType == HIT) stat_write_hit->addData(1);
            else if (resultType == MISS) stat_write_miss->addData(1);
            else if (resultType == NA) stat_write_na->addData(1);
            break;
        case EVICT:
            stat_evict_total->addData(1);
            if (resultType == HIT) stat_evict_hit->addData(1);
            else if (resultType == MISS) stat_evict_miss->addData(1);
            else if (resultType == NA) stat_evict_na->addData(1);
            break;
        case PREFETCH:
            stat_prefetch_total->addData(1);
            if (resultType == HIT) stat_prefetch_hit->addData(1);
            else if (resultType == MISS) stat_prefetch_miss->addData(1);
            else if (resultType == NA) stat_prefetch_na->addData(1);
            break;
    }
}

}}
