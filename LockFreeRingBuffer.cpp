#include "LockFreeRingBuffer.h"

template <class Node>
LockFreeRingBuffer<Node>::LockFreeRingBuffer(size_t num_nodes)
    : _head(1)
    , _tail(0)
    , _num_nodes(num_nodes+1)
    , _ring_buffer((Node*)malloc(_num_nodes * sizeof(Node)))
{}

template <class Node>
LockFreeRingBuffer<Node>::~LockFreeRingBuffer()
{
    free(_ring_buffer);
}

template <class Node>
bool LockFreeRingBuffer<Node>::IsFull() const
{
    return (_tail == _head);
}

template <class Node>
bool LockFreeRingBuffer<Node>::IsEmpty() const
{
    return ((_head + _num_nodes - _tail) % _num_nodes == 1);
}

template <class Node>
Node &LockFreeRingBuffer<Node>::GetAt(size_t pos) const
{
    size_t idx = (_head + pos) % _num_nodes;
    return _ring_buffer[idx];
}

template <class Node>
size_t LockFreeRingBuffer<Node>::Count() const
{
    return ((_head + _num_nodes - _tail) % _num_nodes) - 1;
}

template <class Node>
size_t LockFreeRingBuffer<Node>::Insert(Node *buffer, size_t size, std::function<bool ()> retryWhenFull)
{
    size_t i;
    for(i = 0; i < size; ++i)
    {
        while(IsFull())
        {
            if(retryWhenFull)
            {
                if(!retryWhenFull())
                {
                    return 0;
                }
            }
        }
        _ring_buffer[_head] = buffer[i];
        _head = (_head + 1) % _num_nodes;
    }
    return i;
}

template <class Node>
size_t LockFreeRingBuffer<Node>::Remove(Node *buffer, size_t size, std::function<bool ()> retryWhenEmtpy)
{
    size_t copied = 0;
    for(size_t i = 0; i < size; ++i)
    {
        while(IsEmpty())
        {
            if(retryWhenEmtpy)
            {
                if(!retryWhenEmtpy())
                {
                    return 0;
                }
            }
        }
        _tail = (_tail + 1) % _num_nodes;
        buffer[i] = _ring_buffer[_tail];
        copied ++;
    }
    return copied;
}
