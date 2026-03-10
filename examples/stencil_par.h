#ifndef STENCIL_PAR_H
#define STENCIL_PAR_H

#include "mpi.h"
#include <cstring>

// Macro to convert 2D coordinates to 1D array index
// Assumes arrays have halo zones (bx+2) x (by+2)
#define ind(i,j) ((j)*(bx+2)+(i))

#endif // STENCIL_PAR_H