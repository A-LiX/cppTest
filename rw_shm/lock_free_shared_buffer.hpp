#pragma once
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string>
#include <cstring>
#include <stdexcept>
#include <atomic>
#include <iostream>
#include <cstdint>

// 内存对齐，避免 false sharing
constexpr size_t CACHELINE_SIZE = 64;

// 对齐到 cache line 的 slot
template<typename T>
struct alignas(CACHELINE_SIZE) Slot {
    std::atomic<uint64_t> version;
    T data;
};

template<typename T>
struct SharedRingBuffer {
    std::atomic<size_t> write_index; // 写索引
    std::atomic<size_t> read_index;  // 用于单读者情况（可扩展为数组）
    size_t capacity;
    Slot<T> slots[1]; // 实际大小通过 mmap 扩展
};

template<typename T>
class LockFreeRingBufferWriter {
public:
    LockFreeRingBufferWriter(const std::string& shm_name, size_t capacity)
        : shm_name_(shm_name), capacity_(capacity) {
        size_t size = sizeof(SharedRingBuffer<T>) + (capacity - 1) * sizeof(Slot<T>);
        fd_ = shm_open(shm_name_.c_str(), O_CREAT | O_RDWR, 0666);
        if (fd_ < 0) throw std::runtime_error("shm_open failed");

        ftruncate(fd_, size);
        buffer_ = static_cast<SharedRingBuffer<T>*>(
            mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0));
        if (buffer_ == MAP_FAILED) throw std::runtime_error("mmap failed");

        if (buffer_->capacity != capacity_) {
            buffer_->capacity = capacity_;
            buffer_->write_index.store(0);
            buffer_->read_index.store(0);
            for (size_t i = 0; i < capacity_; ++i) {
                buffer_->slots[i].version.store(0);
            }
        }
    }

    ~LockFreeRingBufferWriter() {
        if (buffer_) munmap(buffer_, mapped_size());
        if (fd_ >= 0) close(fd_);
    }

    void write(const T& data) {
        size_t idx = buffer_->write_index.fetch_add(1, std::memory_order_relaxed) % capacity_;
        uint64_t version = buffer_->slots[idx].version.load(std::memory_order_acquire);

        buffer_->slots[idx].data = data;
        buffer_->slots[idx].version.store(version + 1, std::memory_order_release);
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
class LockFreeRingBufferReader {
public:
    LockFreeRingBufferReader(const std::string& shm_name, size_t capacity)
        : shm_name_(shm_name), capacity_(capacity), local_index_(0), last_version_(0) {
        size_t size = sizeof(SharedRingBuffer<T>) + (capacity - 1) * sizeof(Slot<T>);
        fd_ = shm_open(shm_name_.c_str(), O_RDWR, 0666);
        if (fd_ < 0) throw std::runtime_error("shm_open failed");

        buffer_ = static_cast<SharedRingBuffer<T>*>(
            mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0));
        if (buffer_ == MAP_FAILED) throw std::runtime_error("mmap failed");
    }

    ~LockFreeRingBufferReader() {
        if (buffer_) munmap(buffer_, mapped_size());
        if (fd_ >= 0) close(fd_);
    }

    bool try_read(T& out) {
        size_t idx = local_index_ % capacity_;
        uint64_t version = buffer_->slots[idx].version.load(std::memory_order_acquire);

        if (version > last_version_) {
            out = buffer_->slots[idx].data;
            last_version_ = version;
            local_index_++;
            return true;
        }
        return false;
    }

private:
    size_t mapped_size() const {
        return sizeof(SharedRingBuffer<T>) + (capacity_ - 1) * sizeof(Slot<T>);
    }

    std::string shm_name_;
    size_t capacity_;
    int fd_;
    size_t local_index_;
    uint64_t last_version_;
    SharedRingBuffer<T>* buffer_;
};