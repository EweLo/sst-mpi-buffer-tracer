To use the 2D heat stencil example I used in my thesis, you can follow these steps:
 - Acquire the source code of the 2D heat stencil example and copy the `stencil_mpi_nobuf_optane.cpp` and `stencil_par.h` to this directory.
 - Apply the patch in `stencil.patch` to add the `ariel_enable()` and `ariel_disable()` calls before and after the main loop
    ```
    patch stencil_mpi_nobuf_optane.cpp < stencil.patch
    ```
 - Compile the example with the Makefile in this directory, which will link the stencil against the Ariel API.
    ```
    make
    ```