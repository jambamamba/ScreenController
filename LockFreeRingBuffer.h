#pragma once

#include <atomic>
#include <stdlib.h>
#include <stdint.h>
#include <functional>

template <class Node>
class LockFreeRingBuffer
{
public:
    LockFreeRingBuffer(size_t num_nodes);
    ~LockFreeRingBuffer();
    size_t Insert(Node *buffer, size_t size = 1, std::function<bool ()> retryWhenFull = nullptr);
    size_t Remove(Node *buffer, size_t size = 1, std::function<bool ()> retryWhenEmpty = nullptr);
    Node &GetAt(size_t pos) const;
    size_t Count() const;

protected:
    bool IsFull() const;
    bool IsEmpty() const;

    size_t _head;
    size_t _tail;
    size_t _num_nodes;
    Node *_ring_buffer;
};

