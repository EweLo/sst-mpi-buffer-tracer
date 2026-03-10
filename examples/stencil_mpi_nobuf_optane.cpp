/*
 * Copyright (c) 2012 Torsten Hoefler. All rights reserved.
 *
 * Author(s): Torsten Hoefler <htor@illinois.edu>
 *
 */

#include "stencil_par.h"


#include <string>
#include <iostream>

#ifndef DISABLE_ARIEL_API
#include <arielapi.h>
#endif

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <atomic> //atomics
#include <time.h>
/*#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <linux/perf_event.h>*/

/*static int fd = -1;

void perf_start() {
    struct perf_event_attr pe;
    memset(&pe, 0, sizeof(pe));
    pe.type           = PERF_TYPE_RAW;
    pe.size           = sizeof(pe);
    pe.config         = 0x81d0;  // MEM_INST_RETIRED.ALL_LOADS (event=0xd0, umask=0x81)
    pe.disabled       = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv     = 1;

    fd = syscall(SYS_perf_event_open, &pe, 0, -1, -1, 0);
    if (fd == -1) { perror("perf_event_open"); return; }

    ioctl(fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
}

void perf_stop() {
    ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
    uint64_t count;
    read(fd, &count, sizeof(count));
    printf("MEM_INST_RETIRED.ALL_LOADS: %lu\n", count);
    close(fd);
}*/


void *alloc_shared_mem(std::string shared_memory_folder_prefix, std::string var_name, int size) {
    // Open file on Optane (pmem)
    int fd = open((shared_memory_folder_prefix + var_name + ".bin").c_str(), O_RDWR | O_CREAT, 0666);
    if (fd < 0) {
        std::cerr << "Failed to open file: " << (shared_memory_folder_prefix + var_name + ".bin") << std::endl;
        return NULL;
    }
    ftruncate(fd, size);

    //std::cout << (shared_memory_folder_prefix + var_name + ".bin").c_str() << std::endl;

    // Memory-map the file (DAX)
    int *optane_array = (int *) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (optane_array == MAP_FAILED) {
        std::cerr << "mmap failed\n";
        return NULL;
    }
    // st4d::cout << var_name << ": allocated bytes " << size << std::endl;
    close(fd);
    memset(optane_array, '\0', size);
    return (void *) optane_array;
}

void free_optane(void *optane_array, int size) {
    // Cleanup
    munmap(optane_array, size);
    // close(*fd);
}

std::atomic<int> *alloc_lock(std::string shared_memory_folder_prefix, std::string var_name, int init_value = -1) {
    // Open file on Optane (pmem)
    int fd = open((shared_memory_folder_prefix + "lock_" + var_name + ".bin").c_str(), O_RDWR | O_CREAT, 0666);
    if (fd < 0) {
        std::cerr << "Failed to open file: " << (shared_memory_folder_prefix + "lock_" + var_name + ".bin") <<
                std::endl;
        return NULL;
    }
    int size = sizeof(std::atomic<int>);
    ftruncate(fd, size);
    std::atomic<int> *lock = (std::atomic<int> *) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (lock == MAP_FAILED) {
        std::cerr << "mmap lock failed\n";
        return NULL;
    }
    close(fd);

    if (init_value >= 0) {
        lock->store(init_value, std::memory_order_release);
    }

    return lock;
}

static double get_time() {
    //struct timespec ts;
    //clock_gettime(CLOCK_MONOTONIC, &ts);
    //return ts.tv_sec * 1e3 + ts.tv_nsec / 1e6; // milliseconds
    return 0;
}

////////////


int main(int argc, char **argv) {
    //perf_start();

    bool CXL_REPLACES_UP = false;
    bool CXL_REPLACES_DOWN = false;
    bool CXL_REPLACES_LEFT = false;
    bool CXL_REPLACES_RIGHT = false;

    MPI_Init(&argc, &argv);

    int r, p;
    MPI_Comm comm = MPI_COMM_WORLD;
    MPI_Comm_rank(comm, &r);
    MPI_Comm_size(comm, &p);

    int n, energy, niters, px, py, use_shared_mem, is_shared_mem_optane;

    // locks[r].store(r+100, std::memory_order_release);
    // MPI_Barrier(MPI_COMM_WORLD);
    // std::string output = "";
    // for(int i=0; i<4; i++)
    //   output += std::to_string(locks[i].load(std::memory_order_acquire)) + ";";
    // printf("rank %d: lock vals: %s \n", r, output.c_str());

    if (r == 0) {
        // argument checking
        if (argc < 6) {
            if (!r) printf("usage: stencil_mpi <n> <energy> <niters> <px> <py>\n");

            MPI_Finalize();
            exit(1);
        }

        n = atoi(argv[1]); // nxn grid
        energy = atoi(argv[2]); // energy to be injected per iteration
        niters = atoi(argv[3]); // number of iterations
        px = atoi(argv[4]); // 1st dim processes
        py = atoi(argv[5]); // 2nd dim processes
        use_shared_mem = 0;
        if (argc > 6) {
            use_shared_mem = atoi(argv[6]);
        }
        is_shared_mem_optane = 1;
        if (argc > 7) {
            is_shared_mem_optane = atoi(argv[7]); //1=optane(default), 0=ddr
        }

        if (px * py != p) MPI_Abort(comm, 1); // abort if px or py are wrong
        if (n % py != 0) MPI_Abort(comm, 2); // abort px needs to divide n
        if (n % px != 0) MPI_Abort(comm, 3); // abort py needs to divide n
        // distribute arguments
        int args[7] = {n, energy, niters, px, py, use_shared_mem, is_shared_mem_optane};
        MPI_Bcast(args, 7, MPI_INT, 0, comm);
    } else {
        int args[7];
        MPI_Bcast(args, 7, MPI_INT, 0, comm);
        n = args[0];
        energy = args[1];
        niters = args[2];
        px = args[3];
        py = args[4];
        use_shared_mem = args[5], is_shared_mem_optane = args[6];
    }
    std::string shared_memory_folder_prefix;
    if (is_shared_mem_optane == 1)
        shared_memory_folder_prefix = "/mnt/pmem0/";
    else
        shared_memory_folder_prefix = "/tmp/stepan_stencil/";
    //std::cout << "rank " << r << "  is_shared_mem_optane " << is_shared_mem_optane << "  shared_memory_folder_prefix= " << shared_memory_folder_prefix << std::endl;

    if ((use_shared_mem & 0b1000) != 0)
        CXL_REPLACES_UP = true;
    if ((use_shared_mem & 0b0100) != 0)
        CXL_REPLACES_DOWN = true;
    if ((use_shared_mem & 0b0010) != 0)
        CXL_REPLACES_LEFT = true;
    if ((use_shared_mem & 0b0001) != 0)
        CXL_REPLACES_RIGHT = true;

    if (r == 0) {
        if (use_shared_mem > 0) {
            std::string strinfo = "=== stencil shared memory halos on " + std::string(
                                      is_shared_mem_optane == 1 ? "OPTANE" : "DDR") + " : ";
            if (CXL_REPLACES_UP)
                strinfo += "UP ";
            if (CXL_REPLACES_DOWN)
                strinfo += "DOWN ";
            if (CXL_REPLACES_LEFT)
                strinfo += "LEFT ";
            if (CXL_REPLACES_RIGHT)
                strinfo += "RIGHT ";
            std::cout << strinfo << std::endl;
        } else
            std::cout << "=== stencil using MPI " << std::endl;
    }


    // determine my coordinates (x,y) -- r=x*a+y in the 2d processor array
    int rx = r % px;
    int ry = r / px;
    // determine my four neighbors
    int north = (ry - 1) * px + rx;
    if (ry - 1 < 0) north = MPI_PROC_NULL;
    int south = (ry + 1) * px + rx;
    if (ry + 1 >= py) south = MPI_PROC_NULL;
    int west = ry * px + rx - 1;
    if (rx - 1 < 0) west = MPI_PROC_NULL;
    int east = ry * px + rx + 1;
    if (rx + 1 >= px) east = MPI_PROC_NULL;
    // decompose the domain
    int bx = n / px; // block size in x
    int by = n / py; // block size in y
    int offx = rx * bx; // offset in x
    int offy = ry * by; // offset in y

    //printf("rank: %i, north: %i, south: %i, west: %i, east: %i\n", r, north, south, west, east);

    //printf("%i (%i,%i) - w: %i, e: %i, n: %i, s: %i\n", r, ry,rx,west,east,north,south);

    // allocate two work arrays
    double *aold = (double *) calloc(1, (bx + 2) * (by + 2) * sizeof(double)); // 1-wide halo zones!
    double *anew = (double *) calloc(1, (bx + 2) * (by + 2) * sizeof(double)); // 1-wide halo zones!
    double *tmp;

    // initialize three heat sources
#define nsources 3
    int sources[nsources][2] = {{n / 2, n / 2}, {n / 3, n / 3}, {n * 4 / 5, n * 8 / 9}};
    int locnsources = 0; // number of sources in my area
    int locsources[nsources][2]; // sources local to my rank
    for (int i = 0; i < nsources; ++i) {
        // determine which sources are in my patch
        int locx = sources[i][0] - offx;
        int locy = sources[i][1] - offy;
        if (locx >= 0 && locx < bx && locy >= 0 && locy < by) {
            locsources[locnsources][0] = locx + 1; // offset by halo zone
            locsources[locnsources][1] = locy + 1; // offset by halo zone
            locnsources++;
        }
    }

    double t = get_time(); // take time
    double *sbufnorth, *sbufsouth, *sbufeast, *sbufwest, *rbufnorth, *rbufsouth, *rbufeast, *rbufwest;
    std::atomic<int> *lock_sbufnorth, *lock_sbufsouth, *lock_sbufeast, *lock_sbufwest, *lock_rbufnorth, *lock_rbufsouth,
            *lock_rbufeast, *lock_rbufwest;
    //allocate communication buffers
    if (CXL_REPLACES_UP) //receive from north
    {
        if (north != MPI_PROC_NULL) {
            sbufnorth = (double *) alloc_shared_mem(shared_memory_folder_prefix, std::to_string(north) + "_south",
                                                    bx * sizeof(double));
            lock_sbufnorth = alloc_lock(shared_memory_folder_prefix, std::to_string(north) + "_south");
        }
        if (south != MPI_PROC_NULL) {
            rbufsouth = (double *) alloc_shared_mem(shared_memory_folder_prefix, std::to_string(r) + "_south",
                                                    bx * sizeof(double));
            lock_rbufsouth = alloc_lock(shared_memory_folder_prefix, std::to_string(r) + "_south", 2);
        } else
            rbufsouth = (double *) calloc(1, bx * sizeof(double));
    } else {
        sbufnorth = (double *) calloc(1, bx * sizeof(double)); // send buffers
        rbufsouth = (double *) calloc(1, bx * sizeof(double));
    }
    if (CXL_REPLACES_DOWN) {
        if (south != MPI_PROC_NULL) {
            sbufsouth = (double *) alloc_shared_mem(shared_memory_folder_prefix, std::to_string(south) + "_north",
                                                    bx * sizeof(double));
            lock_sbufsouth = alloc_lock(shared_memory_folder_prefix, std::to_string(south) + "_north");
        }

        if (north != MPI_PROC_NULL) {
            rbufnorth = (double *) alloc_shared_mem(shared_memory_folder_prefix, std::to_string(r) + "_north",
                                                    bx * sizeof(double)); // receive buffers
            lock_rbufnorth = alloc_lock(shared_memory_folder_prefix, std::to_string(r) + "_north", 0);
        } else
            rbufnorth = (double *) calloc(1, bx * sizeof(double)); // receive buffers
    } else {
        sbufsouth = (double *) calloc(1, bx * sizeof(double));
        rbufnorth = (double *) calloc(1, bx * sizeof(double)); // receive buffers
    }
    if (CXL_REPLACES_LEFT) {
        if (west != MPI_PROC_NULL) {
            sbufwest = (double *) alloc_shared_mem(shared_memory_folder_prefix, std::to_string(west) + "_east",
                                                   by * sizeof(double));
            lock_sbufwest = alloc_lock(shared_memory_folder_prefix, std::to_string(west) + "_east");
        }
        if (east != MPI_PROC_NULL) {
            rbufeast = (double *) alloc_shared_mem(shared_memory_folder_prefix, std::to_string(r) + "_east",
                                                   by * sizeof(double));
            lock_rbufeast = alloc_lock(shared_memory_folder_prefix, std::to_string(r) + "_east", 6);
        } else
            rbufeast = (double *) calloc(1, by * sizeof(double));
    } else {
        sbufwest = (double *) calloc(1, by * sizeof(double));
        rbufeast = (double *) calloc(1, by * sizeof(double));
    }
    if (CXL_REPLACES_RIGHT) {
        if (east != MPI_PROC_NULL) {
            sbufeast = (double *) alloc_shared_mem(shared_memory_folder_prefix, std::to_string(east) + "_west",
                                                   by * sizeof(double));
            lock_sbufeast = alloc_lock(shared_memory_folder_prefix, std::to_string(east) + "_west");
        }
        if (west != MPI_PROC_NULL) {
            rbufwest = (double *) alloc_shared_mem(shared_memory_folder_prefix, std::to_string(r) + "_west",
                                                   by * sizeof(double));
            lock_rbufwest = alloc_lock(shared_memory_folder_prefix, std::to_string(r) + "_west", 4);
        } else
            rbufwest = (double *) calloc(1, by * sizeof(double));
    } else {
        sbufeast = (double *) calloc(1, by * sizeof(double));
        rbufwest = (double *) calloc(1, by * sizeof(double));
    }


    MPI_Barrier(MPI_COMM_WORLD); //wait for all locks to be set up
    MPI_Request reqs[8];

    for (int i = 0; i < 8; i++) reqs[i] = MPI_REQUEST_NULL;

    double it_time = get_time();

#ifndef DISABLE_ARIEL_API
    ariel_enable();
    ariel_output_stats();
#endif

    double heat; // total heat in system
    for (int iter = 0; iter < niters; ++iter) {

#ifndef DISABLE_ARIEL_API
        if (r == 0 && (iter % 1 == 0)) {
#else
        if ((iter % 100 == 0)) {
#endif
            double curr_time = get_time();
            std::cout << "Rank " << r << ", iteration " << iter << "/" << niters << ", time " <<  (curr_time - it_time) << std::endl;
            it_time = curr_time;

        }

        // refresh heat sources
        for (int i = 0; i < locnsources; ++i) {
            aold[ind(locsources[i][0], locsources[i][1])] += energy; // heat source
        }

        // std::cout << "======" << r << " - iter " << iter << std::endl;


        if (CXL_REPLACES_UP) {
            if (north != MPI_PROC_NULL) {
                // std::cout << "Rank " << r << " waiting on " << north << "'s south lock to become 2: " << lock_sbufnorth->load(std::memory_order_acquire) << std::endl;
                while (lock_sbufnorth->load(std::memory_order_acquire) != 2);
                for (int i = 0; i < bx; ++i) sbufnorth[i] = aold[ind(i+1, 1)]; // pack loop - last valid region
                // for(int i=0; i<bx; ++i) std::cout << sbufnorth[i] << " ";
                // std::cout << " - north " << r << std::endl;
                // std::cout << "Rank " << r << " set " << north << "'s south lock to 3" << std::endl;
                lock_sbufnorth->store(3, std::memory_order_release);
            }
        } else // MPI UP
        {
            if (north != MPI_PROC_NULL) {
                for (int i = 0; i < bx; ++i) sbufnorth[i] = aold[ind(i+1, 1)]; // pack loop - last valid region
                // for(int i=0; i<bx; ++i) std::cout << sbufnorth[i] << " ";
                // std::cout << " - north " << r << std::endl;
                MPI_Isend(sbufnorth, bx, MPI_DOUBLE, north, 1, comm, &reqs[0]);
            }
            if (south != MPI_PROC_NULL)
                MPI_Irecv(rbufsouth, bx, MPI_DOUBLE, south, 1, comm, &reqs[5]);
        }

        if (CXL_REPLACES_DOWN) {
            if (south != MPI_PROC_NULL) {
                // std::cout << "Rank " << r << " waiting on south lock to become 0: " << lock_sbufsouth->load(std::memory_order_acquire) << std::endl;
                while (lock_sbufsouth->load(std::memory_order_acquire) != 0);
                for (int i = 0; i < bx; ++i) sbufsouth[i] = aold[ind(i+1, by)]; // pack loop
                // for(int i=0; i<bx; ++i) std::cout << sbufsouth[i] << " ";
                // std::cout << " - south " << r << std::endl;
                // std::cout << "Rank " << r << " set " << south << "'s north lock to 1" << std::endl;
                lock_sbufsouth->store(1, std::memory_order_release);
            }
        } else // MPI DOWN
        {
            if (south != MPI_PROC_NULL) {
                for (int i = 0; i < bx; ++i) sbufsouth[i] = aold[ind(i+1, by)]; // pack loop
                // for(int i=0; i<bx; ++i) std::cout << sbufnorth[i] << " ";
                // std::cout << " - south " << r << std::endl;
                MPI_Isend(sbufsouth, bx, MPI_DOUBLE, south, 2, comm, &reqs[1]);
            }
            if (north != MPI_PROC_NULL)
                MPI_Irecv(rbufnorth, bx, MPI_DOUBLE, north, 2, comm, &reqs[4]);
        }
        if (CXL_REPLACES_LEFT) {
            if (west != MPI_PROC_NULL) {
                // std::cout << "Rank " << r << " waiting on west lock to become 6: " << lock_sbufwest->load(std::memory_order_acquire) << std::endl;
                while (lock_sbufwest->load(std::memory_order_acquire) != 6);
                for (int i = 0; i < by; ++i) sbufwest[i] = aold[ind(1, i+1)]; // pack loop
                // for(int i=0; i<by; ++i) std::cout << sbufwest[i] << " ";
                // std::cout << " - west " << r << std::endl;
                // std::cout << "Rank " << r << " set " << west << "'s east lock to 7" << std::endl;
                lock_sbufwest->store(7, std::memory_order_release);
            }
        } else // MPI LEFT
        {
            if (west != MPI_PROC_NULL) {
                for (int i = 0; i < by; ++i) sbufwest[i] = aold[ind(1, i+1)]; // pack loop
                // for(int i=0; i<bx; ++i) std::cout << sbufwest[i] << " ";
                // std::cout << " - west " << r << std::endl;
                MPI_Isend(sbufwest, by, MPI_DOUBLE, west, 4, comm, &reqs[3]);
            }
            if (east != MPI_PROC_NULL)
                MPI_Irecv(rbufeast, by, MPI_DOUBLE, east, 4, comm, &reqs[6]);
        }
        if (CXL_REPLACES_RIGHT) {
            if (east != MPI_PROC_NULL) {
                // std::cout << "Rank " << r << " waiting on east lock to become 4: " << lock_sbufeast->load(std::memory_order_acquire) << std::endl;
                while (lock_sbufeast->load(std::memory_order_acquire) != 4);
                for (int i = 0; i < by; ++i) sbufeast[i] = aold[ind(bx, i+1)]; // pack loop
                // for(int i=0; i<by; ++i) std::cout << sbufeast[i] << " ";
                // std::cout << " - east " << r << std::endl;
                // std::cout << "Rank " << r << " set " << east << "'s west lock to 5" << std::endl;
                lock_sbufeast->store(5, std::memory_order_release);
            }
        } else // MPI RIGHT
        {
            if (east != MPI_PROC_NULL) {
                for (int i = 0; i < by; ++i) sbufeast[i] = aold[ind(bx, i+1)]; // pack loop
                // for(int i=0; i<bx; ++i) std::cout << sbufeast[i] << " ";
                // std::cout << " - east " << r << std::endl;
                MPI_Isend(sbufeast, by, MPI_DOUBLE, east, 3, comm, &reqs[2]);
            }
            if (west != MPI_PROC_NULL)
                MPI_Irecv(rbufwest, by, MPI_DOUBLE, west, 3, comm, &reqs[7]);
        }

        if (CXL_REPLACES_UP) {
            if (south != MPI_PROC_NULL) {
                // std::cout << "Rank " << r << " waiting on south lock to become 3::: " << lock_rbufsouth->load(std::memory_order_acquire) << std::endl;
                while (lock_rbufsouth->load(std::memory_order_acquire) != 3);
            }
        } else // MPI UP
        {
            if (north != MPI_PROC_NULL)
                MPI_Wait(&reqs[0], MPI_STATUSES_IGNORE);
            if (south != MPI_PROC_NULL)
                MPI_Wait(&reqs[5], MPI_STATUSES_IGNORE);
        }
        if (CXL_REPLACES_DOWN) {
            if (north != MPI_PROC_NULL) {
                // std::cout << "Rank " << r << " waiting on north lock to become 1::: " << lock_rbufnorth->load(std::memory_order_acquire) << std::endl;
                while (lock_rbufnorth->load(std::memory_order_acquire) != 1);
            }
        } else //MPI DOWN
        {
            if (south != MPI_PROC_NULL)
                MPI_Wait(&reqs[1], MPI_STATUSES_IGNORE);
            if (north != MPI_PROC_NULL)
                MPI_Wait(&reqs[4], MPI_STATUSES_IGNORE);
        }
        if (CXL_REPLACES_LEFT) {
            if (east != MPI_PROC_NULL) {
                // std::cout << "Rank " << r << " waiting on east lock to become 7::: " << lock_rbufeast->load(std::memory_order_acquire) << std::endl;
                while (lock_rbufeast->load(std::memory_order_acquire) != 7);
            }
        } else // MPI LEFT
        {
            if (west != MPI_PROC_NULL)
                MPI_Wait(&reqs[3], MPI_STATUSES_IGNORE);
            if (east != MPI_PROC_NULL)
                MPI_Wait(&reqs[6], MPI_STATUSES_IGNORE);
        }
        if (CXL_REPLACES_RIGHT) {
            if (west != MPI_PROC_NULL) {
                // std::cout << "Rank " << r << " waiting on west lock to become 5::: " << lock_rbufwest->load(std::memory_order_acquire) << std::endl;
                while (lock_rbufwest->load(std::memory_order_acquire) != 5);
            }
        } else // MPI RIGHT
        {
            if (east != MPI_PROC_NULL)
                MPI_Wait(&reqs[2], MPI_STATUSES_IGNORE);
            if (west != MPI_PROC_NULL)
                MPI_Wait(&reqs[7], MPI_STATUSES_IGNORE);
        }

        // update grid points
        heat = 0.0;
        {
            //j=1
            //i=1
            anew[ind(1, 1)] = aold[ind(1, 1)] / 2.0 + (
                                  rbufwest[0] + aold[ind(1+1, 1)] + rbufnorth[0] + aold[ind(1, 1+1)]) / 4.0 / 2.0;
            heat += anew[ind(1, 1)];
            for (int i = 1 + 1; i < bx + 1 - 1; ++i) {
                anew[ind(i, 1)] = aold[ind(i, 1)] / 2.0 + (
                                      aold[ind(i-1, 1)] + aold[ind(i+1, 1)] + rbufnorth[i - 1] + aold[ind(i, 1+1)]) /
                                  4.0 / 2.0;
                heat += anew[ind(i, 1)];
            }
            //i=bx
            anew[ind(bx, 1)] = aold[ind(bx, 1)] / 2.0 + (
                                   aold[ind(bx-1, 1)] + rbufeast[0] + rbufnorth[bx - 1] + aold[ind(bx, 1+1)]) / 4.0 /
                               2.0;
            heat += anew[ind(bx, 1)];

            for (int j = 1 + 1; j < by + 1 - 1; ++j) {
                //i=1
                anew[ind(1, j)] = aold[ind(1, j)] / 2.0 + (
                                      rbufwest[j - 1] + aold[ind(1+1, j)] + aold[ind(1, j-1)] + aold[ind(1, j+1)]) / 4.0
                                  / 2.0;
                heat += anew[ind(1, j)];
                for (int i = 1 + 1; i < bx + 1 - 1; ++i) {
                    anew[ind(i, j)] = aold[ind(i, j)] / 2.0 + (
                                          aold[ind(i-1, j)] + aold[ind(i+1, j)] + aold[ind(i, j-1)] + aold[ind(i, j+1)])
                                      / 4.0 / 2.0;
                    heat += anew[ind(i, j)];
                }
                //i=bx
                anew[ind(bx, j)] = aold[ind(bx, j)] / 2.0 + (
                                       aold[ind(bx-1, j)] + rbufeast[j - 1] + aold[ind(bx, j-1)] + aold[ind(bx, j+1)]) /
                                   4.0 / 2.0;
                heat += anew[ind(bx, j)];
            }

            //j=by
            //i=1
            anew[ind(1, by)] = aold[ind(1, by)] / 2.0 + (
                                   rbufwest[by - 1] + aold[ind(1+1, by)] + aold[ind(1, by-1)] + rbufsouth[0]) / 4.0 /
                               2.0;
            heat += anew[ind(1, by)];
            for (int i = 1 + 1; i < bx + 1 - 1; ++i) {
                anew[ind(i, by)] = aold[ind(i, by)] / 2.0 + (
                                       aold[ind(i-1, by)] + aold[ind(i+1, by)] + aold[ind(i, by-1)] + rbufsouth[i - 1])
                                   / 4.0 / 2.0;
                heat += anew[ind(i, by)];
            }
            //i=bx
            anew[ind(bx, by)] = aold[ind(bx, by)] / 2.0 + (
                                    aold[ind(bx-1, by)] + rbufeast[by - 1] + aold[ind(bx, by-1)] + rbufsouth[bx - 1]) /
                                4.0 / 2.0;
            heat += anew[ind(bx, by)];
        }

        if (CXL_REPLACES_DOWN && north != MPI_PROC_NULL)
        //wait until the read buffer is ready to read and reset its lock to 0
        {
            // std::cout << "Rank " << r << " set its north lock to 0 => ready to write to" << std::endl;
            lock_rbufnorth->store(0, std::memory_order_release);
        }
        if (CXL_REPLACES_UP && south != MPI_PROC_NULL) {
            // std::cout << "Rank " << r << " set its south lock to 2 => ready to write to" << std::endl;
            lock_rbufsouth->store(2, std::memory_order_release);
        }
        if (CXL_REPLACES_RIGHT && west != MPI_PROC_NULL) {
            // std::cout << "Rank " << r << " set its west lock to 4 => ready to write to" << std::endl;
            lock_rbufwest->store(4, std::memory_order_release);
        }
        if (CXL_REPLACES_LEFT && east != MPI_PROC_NULL) {
            // std::cout << "Rank " << r << " set its east lock to 6 => ready to write to" << std::endl;
            lock_rbufeast->store(6, std::memory_order_release);
        }

        // swap arrays
        tmp = anew;
        anew = aold;
        aold = tmp;

        // optional - print image
        //if(iter == niters-1) printarr_par(iter, anew, n, px, py, rx, ry, bx, by, offx, offy, comm);
    }
    t -= get_time();

#ifndef DISABLE_ARIEL_API
    ariel_output_stats();
    ariel_disable();
#endif

    // get final heat in the system
    double rheat;
    MPI_Allreduce(&heat, &rheat, 1, MPI_DOUBLE, MPI_SUM, comm);
    if (!r) printf("[%i] last heat: %f time: %f\n", r, rheat, t);

    MPI_Finalize();

    //perf_stop();
}