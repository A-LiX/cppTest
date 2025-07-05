#pragma once
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <pthread.h>
#include <string>
#include <cstring>
#include <stdexcept>
#include <atomic>
#include <iostream>

template<typename T>
struct Slot {
    uint64_t version;
    T data;
};

template<typename T>
struct SharedRingBuffer {
    pthread_rwlock_t rwlock;
    size_t capacity;
    size_t write_index;
    uint64_t global_version;
    Slot<T> slots[1]; // 实际大小通过 mmap 扩展
};

template<typename T>
class SharedRingBufferWriter {
public:
    SharedRingBufferWriter(const std::string& shm_name, size_t capacity)
        : shm_name_(shm_name), capacity_(capacity) {
        size_t size = sizeof(SharedRingBuffer<T>) + (capacity - 1) * sizeof(Slot<T>);
        fd_ = shm_open(shm_name_.c_str(), O_CREAT | O_RDWR, 0666);
        if (fd_ < 0) throw std::runtime_error("shm_open failed");

        ftruncate(fd_, size);
        buffer_ = static_cast<SharedRingBuffer<T>*>(mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0));
        if (buffer_ == MAP_FAILED) throw std::runtime_error("mmap failed");

        // 初始化（只初始化一次）
        if (buffer_->capacity != capacity_) {
            pthread_rwlockattr_t attr;
            pthread_rwlockattr_init(&attr);
            pthread_rwlockattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
            pthread_rwlock_init(&buffer_->rwlock, &attr);
            pthread_rwlockattr_destroy(&attr);

            buffer_->capacity = capacity_;
            buffer_->write_index = 0;
            buffer_->global_version = 0;
        }
    }

    ~SharedRingBufferWriter() {
        if (buffer_) munmap(buffer_, mapped_size());
        if (fd_ >= 0) close(fd_);
    }

    void write(const T& data) {
        pthread_rwlock_wrlock(&buffer_->rwlock);

        size_t idx = buffer_->write_index % buffer_->capacity;
        buffer_->slots[idx].data = data;
        buffer_->slots[idx].version = ++buffer_->global_version;
        buffer_->write_index++;

        pthread_rwlock_unlock(&buffer_->rwlock);
    }

private:
    size_t mapped_size() const {
        return sizeof(SharedRingBuffer<T>) + (capacity_ - 1) * sizeof(Slot<T>);
    }

    std::string shm_name_;
    size_t capacity_;
    int fd_;
    SharedRingBuffer<T>* buffer_;
};

template<typename T>
class SharedRingBufferReader {
public:
    SharedRingBufferReader(const std::string& shm_name, size_t capacity)
        : shm_name_(shm_name), capacity_(capacity), last_version_(0) {
        size_t size = sizeof(SharedRingBuffer<T>) + (capacity - 1) * sizeof(Slot<T>);
        fd_ = shm_open(shm_name_.c_str(), O_RDWR, 0666);
        if (fd_ < 0) throw std::runtime_error("shm_open failed");

        buffer_ = static_cast<SharedRingBuffer<T>*>(mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0));
        if (buffer_ == MAP_FAILED) throw std::runtime_error("mmap failed");
    }

    ~SharedRingBufferReader() {
        if (buffer_) munmap(buffer_, mapped_size());
        if (fd_ >= 0) close(fd_);
    }

    // 读取最新一条大于 last_version 的数据（如果有）
    bool try_read(T& out) {
        bool found = false;
        pthread_rwlock_rdlock(&buffer_->rwlock);
        for (size_t i = 0; i < buffer_->capacity; ++i) {
            if (buffer_->slots[i].version > last_version_) {
                out = buffer_->slots[i].data;
                last_version_ = buffer_->slots[i].version;
                found = true;
            }
        }
        pthread_rwlock_unlock(&buffer_->rwlock);
        return found;
    }

private:
    size_t mapped_size() const {
        return sizeof(SharedRingBuffer<T>) + (capacity_ - 1) * sizeof(Slot<T>);
    }

    std::string shm_name_;
    size_t capacity_;
    int fd_;
    uint64_t last_version_;
    SharedRingBuffer<T>* buffer_;
};