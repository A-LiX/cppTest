#include <atomic>
#include <cstddef>
#include <iostream>
#include <thread>
#include <vector>

// MPMC Lock-Free Ring Buffer
template <typename T, size_t Capacity>
class LockFreeMPMCRingBuffer
{
private:
    struct Slot
    {
        std::atomic<size_t> sequence;
        T data;
    };

    static constexpr size_t kSize = Capacity;
    Slot buffer[kSize];

    std::atomic<size_t> head{0};
    std::atomic<size_t> tail{0};

public:
    LockFreeMPMCRingBuffer()
    {
        for (size_t i = 0; i < kSize; ++i)
        {
            buffer[i].sequence.store(i, std::memory_order_relaxed);
        }
    }

    inline bool enqueue(const T &value)
    {
        size_t pos = tail.load(std::memory_order_relaxed);

        while (true)
        {
            Slot &slot = buffer[pos % kSize];
            size_t seq = slot.sequence.load(std::memory_order_acquire);

            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
            if (diff == 0)
            {
                if (tail.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                {
                    slot.data = value;
                    slot.sequence.store(pos + 1, std::memory_order_release);
                    return true;
                }
            }
            else if (diff < 0)
            {
                return false; // 满了
            }
            else
            {
                pos = tail.load(std::memory_order_relaxed); // retry
            }
        }
    }

    inline bool dequeue(T &result)
    {
        size_t pos = head.load(std::memory_order_relaxed);

        while (true)
        {
            Slot &slot = buffer[pos % kSize];
            size_t seq = slot.sequence.load(std::memory_order_acquire);

            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);
            if (diff == 0)
            {
                if (head.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                {
                    result = slot.data;
                    slot.sequence.store(pos + kSize, std::memory_order_release);
                    return true;
                }
            }
            else if (diff < 0)
            {
                return false; // 空
            }
            else
            {
                pos = head.load(std::memory_order_relaxed); // retry
            }
        }
    }
};

// MPSC Lock-Free Ring Buffer
template <typename T, size_t Capacity>
class LockFreeMPSCQueue
{
private:
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");

    T buffer[Capacity];
    std::atomic<size_t> head{0}; // 由消费者线程独占访问
    std::atomic<size_t> tail{0}; // 多个生产者共享访问

public:
    inline bool enqueue(const T &value)
    {
        size_t pos = tail.fetch_add(1, std::memory_order_acq_rel);
        if (pos - head.load(std::memory_order_acquire) >= Capacity)
        {
            return false; // 队列满
        }
        buffer[pos % Capacity] = value;
        return true;
    }

    inline bool dequeue(T &result)
    {
        size_t h = head.load(std::memory_order_relaxed);
        if (h >= tail.load(std::memory_order_acquire))
        {
            return false; // 队列空
        }
        result = buffer[h % Capacity];
        head.store(h + 1, std::memory_order_release);
        return true;
    }
};

// SPSC Lock-Free Ring Queue
template <typename T, size_t Capacity>
class LockFreeSPSCQueue
{
private:
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of two");

    T buffer[Capacity];
    size_t head = 0;             // 消费者线程独占访问
    std::atomic<size_t> tail{0}; // 生产者线程独占写入，但消费者读取

public:
    inline bool enqueue(const T &value)
    {
        size_t t = tail.load(std::memory_order_relaxed);
        size_t next_t = (t + 1) % Capacity;
        if (next_t == head)
        {
            return false; // 满
        }
        buffer[t] = value;
        tail.store(next_t, std::memory_order_release);
        return true;
    }

    inline bool dequeue(T &result)
    {
        if (head == tail.load(std::memory_order_acquire))
        {
            return false; // 空
        }
        result = buffer[head];
        head = (head + 1) % Capacity;
        return true;
    }
};
