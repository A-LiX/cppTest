#ifndef COMMON_H
#define COMMON_H

#include <pthread.h>
#include <cstdint>
#include <cstddef>

const char* SHM_NAME = "/my_ring_buffer";
const size_t RING_CAPACITY = 128;
const size_t DATA_SIZE = 256;

struct Slot {
    uint64_t version;
    char data[DATA_SIZE];
};

struct RingBuffer {
    pthread_rwlock_t rwlock;
    size_t capacity;
    size_t write_index;
    uint64_t global_version;
    Slot slots[RING_CAPACITY];
};

#endif