CXX=$(shell sst-config --CXX)
CXXFLAGS=$(shell sst-config --ELEMENT_CXXFLAGS)
LDFLAGS=$(shell sst-config --ELEMENT_LDFLAGS)

libtracerPortModule.so: tracerPortModule.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $<

install:
	sst-register tracerPortModule tracerPortModule_LIBDIR=$(CURDIR)
	sst-register SST_ELEMENT_SOURCE tracerPortModule=$(CURDIR)