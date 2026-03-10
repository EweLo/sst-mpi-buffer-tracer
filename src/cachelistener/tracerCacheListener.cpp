#include "tracerCacheListener.h"

#include <fcntl.h>
#include <sys/mman.h>
#include "../tracercomponent/customTracer.h"

TracerCacheListener::TracerCacheListener(ComponentId_t id, Params &params) : CacheListener(id, params) {
    out = new Output("TracerCacheListener[@f:@l:@p] ", 1, 0, Output::STDOUT);
    
    out->verbose(CALL_INFO, 1, 0, "Tracer CacheListener was constructed for Cache '%s'.\n", getParentComponentName().c_str());

    //initConnectedPortModule(false); // May not succeed, as the portModule may not be constructed yet. In that case we will try again later in notifyAccess().
}

void TracerCacheListener::notifyAccess(const MemHierarchy::CacheListenerNotification& notify) {
    //  We do this here because at construction or setup time, the portModule may not be constructed yet.
    /*if (__builtin_expect(connectedPortModule == nullptr, 0)) { // We use __builtin_expect as this should be true only once
        initConnectedPortModule(true);
    }*/

#ifdef EXTENDED_CACHE_LISTENER

    const MemHierarchy::NotifyAccessType notifyAccessType = notify.getAccessType();
    const MemHierarchy::NotifyResultType notifyResType = notify.getResultType();
    const MemHierarchy::Addr addr = notify.getPhysicalAddress();

    if (notifyAccessType == MemHierarchy::READ || notifyAccessType == MemHierarchy::WRITE) {
        if (notifyResType == MemHierarchy::HIT) {
            //out->verbose(CALL_INFO, 1, 0, "%s HIT for address: 0x%" PRIx64 "\n", notifyAccessType == MemHierarchy::READ ? "READ" : "WRITE", addr);
            if (notify.wasLinePrefetched()) {
                //out->verbose(CALL_INFO, 1, 0, "  --> This line was prefetched before being accessed. Event ID: %lu\n", notify.getEventID().first);
                CustomTracer::markAsPrefetched(notify.getEventID());
            }
        }
    }

#else
    out->fatal(CALL_INFO, -1, "To be able to trace prefetched cache hits, the tracerCacheListener needs to be compiled and run with the custom fork of SST-Elements (https://github.com/EweLo/sst-elements/tree/extend-cachelistener).\n");

#endif

    /*if (notifyAccessType == MemHierarchy::PREFETCH) {
        tracedCacheLines[addr] = true;

        //out->verbose(CALL_INFO, 1, 0, "PREFETCH for address: 0x%" PRIx64 "\n", addr);
    }

    if (notifyAccessType == MemHierarchy::EVICT) {
        tracedCacheLines[addr] = false;

        //out->verbose(CALL_INFO, 1, 0, "EVICT for address: 0x%" PRIx64 "\n", addr);
    }

    if (notifyAccessType == MemHierarchy::READ || notifyAccessType == MemHierarchy::WRITE) {

        if (notifyResType == MemHierarchy::MISS) {
            //out->verbose(CALL_INFO, 1, 0, "%s MISS for address: 0x%" PRIx64 "\n", notifyAccessType == MemHierarchy::READ ? "READ" : "WRITE", addr);

            tracedCacheLines[addr] = false;
        } else if (notifyResType == MemHierarchy::HIT) {
            //out->verbose(CALL_INFO, 1, 0, "%s HIT for address: 0x%" PRIx64 "\n", notifyAccessType == MemHierarchy::READ ? "READ" : "WRITE", addr);

            auto it = tracedCacheLines.find(addr);

            if (it != tracedCacheLines.end() && it->second) {
                // cache line is prefetched
                //out->verbose(CALL_INFO, 1, 0, "  --> This line was prefetched before being accessed.\n");

                // should we keep the line as 'prefetched'? -> NO
                tracedCacheLines[addr] = false;
            } else {
                // cache line is not prefetched
                // out->verbose(CALL_INFO, 1, 0, "  --> This line was not prefetched before being accessed.\n");
            }
        }
    }*/
}

/*inline void TracerCacheListener::initConnectedPortModule(bool throwErr) {
    connectedPortModule = TracerPortModule::getTracerPortModuleForComponent(getParentComponentName());
    if (connectedPortModule == nullptr) {
        if (throwErr) {
            out->fatal(CALL_INFO, 1, "Could not find TracerPortModule instance for component '%s'. Make sure that the component has a TracerPortModule attached.\n", getParentComponentName().c_str());
        }
    }
    out->verbose(CALL_INFO, 1, 0, "Successfully connected to TracerPortModule of component '%s'.\n", getParentComponentName().c_str());
}*/

/*inline MemHierarchy::MemEvent* TracerCacheListener::getMemEventFromPortModule(const MemHierarchy::CacheListenerNotification& notify) {
    auto *pendingEventsQueue = connectedPortModule->getPendingEventsQueue();

    MemHierarchy::MemEvent *me = pendingEventsQueue->front();
    if (me == nullptr) {
        out->verbose(CALL_INFO, 1, 0, "No pending MemEvent found in PortModule for CacheListener notification.\n");
        return nullptr;
    }

    return nullptr;
}*/