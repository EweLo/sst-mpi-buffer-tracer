//
// Created by ewelo on 19.01.26.
//
#define _XOPEN_SOURCE 700

#include "tracer_ipc.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

static int __tunnel_init(void **tunnel, size_t size, const char *shm_name, bool create) {
    int tunnel_fd;

    if (!tunnel) {
        return -1;
    }

    if (create) {
        // Create and initialize tunnel
        tunnel_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
        if (tunnel_fd == -1) {
            fprintf(stderr, "%s:%d (%s) shm_open: %s\n", __FILE__, __LINE__, __func__, strerror(errno));
            return -1;
        }

        // Set size
        if (ftruncate(tunnel_fd, size) == -1) {
            fprintf(stderr, "%s:%d (%s) ftruncate: %s\n", __FILE__, __LINE__, __func__, strerror(errno));
            close(tunnel_fd);
            return -1;
        }
    } else {
        // Open existing tunnel
        tunnel_fd = shm_open(shm_name, O_RDWR, 0666);
        if (tunnel_fd == -1) {
            fprintf(stderr, "%s:%d (%s) shm_open: %s\n", __FILE__, __LINE__, __func__, strerror(errno));
            return -1;
        }
    }

    // Map to process address space
    *tunnel = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, tunnel_fd, 0);
    close(tunnel_fd);

    if (*tunnel == MAP_FAILED) {
        fprintf(stderr, "%s:%d (%s) mmap: %s\n", __FILE__, __LINE__, __func__, strerror(errno));
        *tunnel = NULL;
        return -1;
    }

    return 0;
}

static int __init_tunnel_mutex(pthread_mutex_t *m) {
    pthread_mutexattr_t attr;

    if (pthread_mutexattr_init(&attr) != 0) return -1;

    if (pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) != 0 ) {
        pthread_mutexattr_destroy(&attr);
        return -1;
    }

    /*if (pthread_mutexattr_setrobust (&attr, PTHREAD_MUTEX_ROBUST) != 0) {
        pthread_mutexattr_destroy(&attr);
        return -1;
    }*/

    int ret = pthread_mutex_init(m, &attr);
    pthread_mutexattr_destroy(&attr);
    return ret;
}

int tunnel_mpi_traces_init(MpiTracesTunnel **tunnel, bool create) {
    int ret = __tunnel_init((void **)tunnel, sizeof(MpiTracesTunnel), SHM_NAME_MPI, create);

    if (ret == 0 && create && *tunnel) {
        // Initialize each producer's queue
        for (unsigned int i = 0; i < MAX_PRODUCERS; i++) {
            ProducerQueueMpi *q = &(*tunnel)->queues[i];
            q->head = 0;
            q->tail = 0;

            if (__init_tunnel_mutex(&q->lock) != 0) {
                fprintf(stderr, "%s:%d (%s) mutex init failed for queue %u: %s\n", __FILE__, __LINE__, __func__, i, strerror(errno));
                return -1;
            }
        }
    }

    return ret;
}

int tunnel_simple_mpi_traces_init(SimpleMpiTracesTunnel **tunnel, bool create) {
    int ret = __tunnel_init((void **)tunnel, sizeof(SimpleMpiTracesTunnel), SHM_NAME_MPI_SIMPLE, create);

    if (ret == 0 && create && *tunnel) {
        // Initialize each producer's queue
        for (unsigned int i = 0; i < MAX_PRODUCERS; i++) {
            ProducerQueueSimple *q = &(*tunnel)->queues[i];
            q->head = 0;
            q->tail = 0;

            if (__init_tunnel_mutex(&q->lock) != 0) {
                fprintf(stderr, "%s:%d (%s) mutex init failed for queue %u: %s\n", __FILE__, __LINE__, __func__, i, strerror(errno));
                return -1;
            }
        }
    }

    return ret;
}

int tunnel_mpi_traces_send(MpiTracesTunnel* tunnel, MpiTrace* trace, unsigned int producer_id) {
    if (tunnel == NULL || trace == NULL) {
        return -1;
    }

    if (producer_id >= MAX_PRODUCERS) {
        fprintf(stderr, "%s:%d (%s) Invalid producer_id: %u\n", __FILE__, __LINE__, __func__, producer_id);
        return -1;
    }

    ProducerQueueMpi *queue = &tunnel->queues[producer_id];

    // Blocking lock the queue for this producer
    if (pthread_mutex_lock(&queue->lock) != 0) {
        return -1;
    }

    unsigned int next_head = (queue->head + 1) & (RING_BUFFER_SIZE - 1);

    // Check if buffer is full
    if (next_head == queue->tail) {
        pthread_mutex_unlock(&queue->lock);
        fprintf(stderr, "%s:%d (%s) Queue full for producer %u\n", __FILE__, __LINE__, __func__, producer_id);
        return -1;
    }

    // Copy trace to the ring buffer
    memcpy(&queue->traces[queue->head], trace, sizeof(MpiTrace));

    queue->head = next_head;

    // Unlock the queue
    pthread_mutex_unlock(&queue->lock);

    return 0;
}

int tunnel_simple_mpi_traces_send(SimpleMpiTracesTunnel* tunnel, SimpleMpiTrace* trace, unsigned int producer_id) {
    if (tunnel == NULL || trace == NULL) {
        return -1;
    }

    if (producer_id >= MAX_PRODUCERS) {
        fprintf(stderr, "%s:%d (%s) Invalid producer_id: %u\n", __FILE__, __LINE__, __func__, producer_id);
        return -1;
    }

    ProducerQueueSimple *queue = &tunnel->queues[producer_id];

    // Blocking lock the queue for this producer
    if (pthread_mutex_lock(&queue->lock) != 0) {
        return -1;
    }

    unsigned int next_head = (queue->head + 1) & (RING_BUFFER_SIZE - 1);

    // Check if buffer is full
    if (next_head == queue->tail) {
        pthread_mutex_unlock(&queue->lock);
        fprintf(stderr, "%s:%d (%s) Queue full for producer %u\n", __FILE__, __LINE__, __func__, producer_id);
        return -1;
    }

    // Copy trace to the ring buffer
    memcpy(&queue->traces[queue->head], trace, sizeof(SimpleMpiTrace));

    queue->head = next_head;

    // Unlock the queue
    pthread_mutex_unlock(&queue->lock);

    return 0;
}

int tunnel_mpi_traces_recv(MpiTracesTunnel* tunnel, MpiTrace* trace, unsigned int *producer_id) {
    if (tunnel == NULL || trace == NULL) {
        return -1;
    }

    static unsigned int last_checked = 0;

    // Round-robin through producer queues, skip queues that are currently locked by producers
    for (unsigned int i = 0; i < MAX_PRODUCERS; i++) {
        unsigned int idx = (last_checked + i) % MAX_PRODUCERS;
        ProducerQueueMpi *queue = &tunnel->queues[idx];

        // Try to lock the queue, skip if it's currently locked by producer
        if (pthread_mutex_trylock(&queue->lock) != 0) {
            continue;
        }

        // Check if this queue has data
        if (queue->head != queue->tail) {
            // Copy trace from the ring buffer
            memcpy(trace, &queue->traces[queue->tail], sizeof(MpiTrace));

            queue->tail = (queue->tail + 1) & (RING_BUFFER_SIZE - 1);

            // Unlock the queue
            pthread_mutex_unlock(&queue->lock);

            last_checked = (idx + 1) % MAX_PRODUCERS;
            return 0;
        }

        // Unlock the queue
        pthread_mutex_unlock(&queue->lock);
    }

    return -1;
}

int tunnel_simple_mpi_traces_recv(SimpleMpiTracesTunnel* tunnel, SimpleMpiTrace* trace, unsigned int *producer_id) {
    if (tunnel == NULL || trace == NULL) {
        return -1;
    }

    static unsigned int last_checked = 0;

    // Round-robin through producer queues, skip queues that are currently locked by producers
    for (unsigned int i = 0; i < MAX_PRODUCERS; i++) {
        unsigned int idx = (last_checked + i) % MAX_PRODUCERS;
        ProducerQueueSimple *queue = &tunnel->queues[idx];

        // Try to lock the queue, skip if it's currently locked by producer
        if (pthread_mutex_trylock(&queue->lock) != 0) {
            continue;
        }

        // Check if this queue has data
        if (queue->head != queue->tail) {
            // Copy trace from the ring buffer
            memcpy(trace, &queue->traces[queue->tail], sizeof(SimpleMpiTrace));

            queue->tail = (queue->tail + 1) & (RING_BUFFER_SIZE - 1);

            // Unlock the queue
            pthread_mutex_unlock(&queue->lock);

            last_checked = (idx + 1) % MAX_PRODUCERS;
            return 0;
        }

        // Unlock the queue
        pthread_mutex_unlock(&queue->lock);
    }

    return -1;
}