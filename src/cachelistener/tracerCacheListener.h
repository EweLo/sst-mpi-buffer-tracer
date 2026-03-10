#ifndef SST_CUSTOM_TRACER_TRACERCACHELISTENER_H
#define SST_CUSTOM_TRACER_TRACERCACHELISTENER_H

#include <semaphore.h>
#include <sst/elements/memHierarchy/cacheListener.h>
#include "../portmodule/tracerPortModule.h"

class TracerCacheListener : public MemHierarchy::CacheListener {
public:
    SST_ELI_REGISTER_SUBCOMPONENT(
        TracerCacheListener,
        "customTracer",
        "tracerCacheListener",
        SST_ELI_ELEMENT_VERSION(1,0,0),
        "Cache listener for tracer",
        SST::MemHierarchy::CacheListener
    )

    TracerCacheListener(ComponentId_t id, Params& params);

    void notifyAccess(const MemHierarchy::CacheListenerNotification& notify) override;

private:
    Output *out;
    //std::unordered_map<MemHierarchy::Addr, bool> tracedCacheLines;

    //TracerPortModule *connectedPortModule = nullptr;

    //inline void initConnectedPortModule(bool throwErr);
    //inline MemHierarchy::MemEvent* getMemEventFromPortModule(const MemHierarchy::CacheListenerNotification& notify);
};


#endif //SST_CUSTOM_TRACER_TRACERCACHELISTENER_H