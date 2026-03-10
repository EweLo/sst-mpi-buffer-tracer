CC := $(shell sst-config --CC)
CFLAGS := $(shell mpicc --showme:compile) -std=c11 -Isrc
CXX := $(shell sst-config --CXX)
CXXFLAGS := $(shell sst-config --ELEMENT_CXXFLAGS) -O3 -I $(SST_ELEMENTS_HOME)/include $(shell mpicxx --showme:compile) -Isrc -Isrc/cachelistener -Isrc/portmodule -Isrc/tracercomponent -Isrc/mpitracer
LDFLAGS := $(shell sst-config --ELEMENT_LDFLAGS)

MPICC = mpicc
BUILD_DIR = build

ARIEL_API_LIB_DIR = $(SST_ELEMENTS_HOME)/lib
ARIEL_API_INCLUDE_DIR = $(SST_ELEMENTS_HOME)/include/sst/elements/ariel/api

.PHONY: all install clean examples

all: $(BUILD_DIR) $(BUILD_DIR)/libcustomTracer.so $(BUILD_DIR)/libmpi_trace.so $(BUILD_DIR)/mpilauncher

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/libcustomTracer.so: $(BUILD_DIR)/customTracer.o $(BUILD_DIR)/tracerPortModule.o $(BUILD_DIR)/tracer_ipc.o $(BUILD_DIR)/tracerCacheListener.o $(BUILD_DIR)/perfCacheListener.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

$(BUILD_DIR)/tracerCacheListener.o: src/cachelistener/tracerCacheListener.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -fPIC -o $@ $<

$(BUILD_DIR)/perfCacheListener.o: src/cachelistener/perfCacheListener.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -fPIC -o $@ $<

$(BUILD_DIR)/tracerPortModule.o: src/portmodule/tracerPortModule.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -fPIC -o $@ $<

$(BUILD_DIR)/customTracer.o: src/tracercomponent/customTracer.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -fPIC -o $@ $<

install:
	sst-register customTracer customTracer_LIBDIR=$(CURDIR)/$(BUILD_DIR)
	sst-register SST_ELEMENT_SOURCE customTracer=$(CURDIR)

examples:
	(cd examples && $(MAKE) all)

$(BUILD_DIR)/libmpi_trace.so: src/mpitracer/mpi_trace.c $(BUILD_DIR)/tracer_ipc.o | $(BUILD_DIR)
	$(MPICC) -shared -fPIC -Wl,-rpath,$(ARIEL_API_LIB_DIR) -o $@ $^ -I$(ARIEL_API_INCLUDE_DIR) -L$(ARIEL_API_LIB_DIR) -larielapi -fopenmp

$(BUILD_DIR)/mpilauncher: src/mpitracer/mpilauncher.cc | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $<

$(BUILD_DIR)/tracer_ipc.o: src/tracer_ipc.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -fPIC -c -o $@ $<

clean:
	rm -rf $(BUILD_DIR)
