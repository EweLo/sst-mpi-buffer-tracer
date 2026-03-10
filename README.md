This repository contains the code of the implementation for my Bachelor's thesis,
written at the [Chair of Computer Architecture and Parallel Systems](https://www.ce.cit.tum.de/en/caps/homepage/) at the Technical University of Munich (TUM).

# SST MPI Buffer Load Tracer
Custom component for the Structural Simulation Toolkit (SST) for tracing memory load accesses to MPI buffers,
generating traces comparable to the ones collected with [Mitos](https://github.com/caps-tum/mitos/tree/mpi-tracing) on real hardware.

## Installation

### 1. Install SST
Install `sst-core` and `sst-elements` v15.1.0 according to the [detailed installation instructions on the SST website](https://sst-simulator.org/SSTPages/SSTBuildAndInstall_15dot1dot0_SeriesDetailedBuildInstructions/).\
This element should also work with newer versions of SST, but it has only been tested with v15.1.0.

1. Make sure you configure sst-core with MPI support **disabled** by using the `--disable-mpi` flag when running `./configure`, as Ariel currently only support instrumenting MPI applications if SST-core is built **without** MPI support.
   - Ariel [will receive support for SST-core built with MPI](https://github.com/sstsimulator/sst-elements/pull/2638) in the future; however, I'm not sure whether this would be compatible with my implementation, so I would still recommend building SST-core without MPI support for now.
   
2. Make sure you **enable** Ariel MPI Support with the `--enable-ariel-mpi` flag when configuring sst-elements and also make sure you compile Ariel with **Intel Pin Tool 3.31** (as explained on [their website](https://sst-simulator.org/SSTPages/SSTBuildAndInstall_15dot1dot0_SeriesAdditionalExternalComponents/)).

> [!CAUTION]\
> With sst-elements v15.1.0 (and possibly also newer versions), the Ariel API Makefile is not properly implemented, which causes Ariel to be always build wihout MPI support, even if the `--enable-ariel-mpi` flag is set.
> This means you have to manually edit the `src/sst/elements/ariel/api/Makefile.am` in sst-elements, as described in [this issue](https://github.com/sstsimulator/sst-elements/issues/2624) I opened, before running `./configure`.

3. Make sure you have correctly set the `SST_ELEMENTS_HOME` environment variable, as described in the SST installation instructions.

### 2. Install the SST MPI Buffer Load Tracer
1. Clone this repository 
   ```
   git clone https://github.com/EweLo/sst-mpi-buffer-tracer.git
   cd sst-mpi-buffer-tracer
   ```
2. Build the element and other libraries 
   ```
   make all
   ```
3. Install and register the element with SST
   ```
   make install
   ```
4. Check if the installation was successful by running
   ```
   sst-info customTracer
   ```
   
## Usage
1. Compile the target MPI application as usual but make sure to link it against the Ariel API.
   - You can find an example Makefile in the `example/` directory of this repository or in the [tests of Ariel in sst-elements](https://github.com/sstsimulator/sst-elements/tree/master/src/sst/elements/ariel/tests/testMPI).
   - For using the 2D heat stencil example, that I used in my thesis, read the instructions in the `example/` directory of this repository.

2. Run the simulation configured in `sst-simulation-configurable.py` with SST
   ```
   sst sst-simulation-configurable.py -- -r <nranks> <binary> -- [args ...]
   ```
   where `<nranks>` is the number of MPI ranks you want the target application, `<binary>` is the path to the compiled target application and `[args ...]` are the command line arguments for the target application.
    - You can configure which MPI rank should be instrumented by Ariel with the `-t` flag.

### Configuring the simulation
The SST simulation can be configured in two ways:
1. By editing the `sst-simulation-configurable.py` file directly and changing the parms of the different components.
2. By creating a JSON file with the desired configuration and passing it to the simulation with the `-c` flag, e.g.\
   ```
   sst sst-simulation-configurable.py -- -c config.json -i 0 -r <nranks> <binary> -- [args ...]
   ```
   where `congig.json` is a JSON file containing the desired configuration of the simulation and `-i` specifying the index of the configuration to use (the JSON file can contain multiple configurations).\
    - You can find an example JSON configuration in the `model_configs_example.json` file in this repository.
    - The JSON configuration file will only override the parameters specified in the JSON file, so you can also use it to only change a few parameters of the simulation and keep the rest of the default configuration in the `sst-simulation-configurable.py` file.

> [!NOTE]\
> If you plan to use one of the cache listeners in `src/cachelistener` with sst-elements v15.1.0 or older, make sure you manually apply the patch in [this pull request](https://github.com/sstsimulator/sst-elements/pull/2619),
> otherwise no cache listeners will be loaded.
> This is not required for the normal usage of the tracer and was only used for validating the tracer in my thesis (`src/cachelistener/perfCacheListener.cpp`)
> or for testing whether it's possible to detect prefetches with the tracer (`src/cachelistener/tracerCacheListener.cpp`).