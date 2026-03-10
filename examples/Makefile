CC = gcc
CFLAGS = -O3 -Wall -Wextra -std=c11
CXXFLAGS = -O3 -Wall -Wextra -std=c++11
MPICC = mpicc
MPICXX = mpicxx

ARIEL_API_LIB_DIR = $(SST_ELEMENTS_HOME)/lib 
ARIEL_API_INCLUDE_DIR = $(SST_ELEMENTS_HOME)/include/sst/elements/ariel/api

LD_MITOSHOOKS=-lmitoshooks -linstructionAPI -lparseAPI -lsymtabAPI

.PHONY: all clean mpi-stencil mpi-stencil-mitos

all: mpi-stencil

mpi-stencil: stencil_mpi_nobuf_optane.cpp
	$(MPICXX) $(CXXFLAGS) -Wl,-rpath,$(ARIEL_API_LIB_DIR) -o $@ $^ -I$(ARIEL_API_INCLUDE_DIR) -L$(ARIEL_API_LIB_DIR) -larielapi -fopenmp

mpi-stencil-mitos: stencil_mpi_nobuf_optane.cpp
	$(MPICXX) $(CXXFLAGS) -o $@ $^ $(LD_MITOSHOOKS) -fopenmp -DDISABLE_ARIEL_API

clean:
	@rm -f mpi-stencil mpi-stencil-mitos *.o
