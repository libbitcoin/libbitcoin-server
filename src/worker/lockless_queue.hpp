#ifndef OBELISK_WORKER_LOCKLESS_QUEUE_HPP
#define OBELISK_WORKER_LOCKLESS_QUEUE_HPP

#include <atomic>

namespace obelisk {
 
/**
 * Lockless queue. Multiple producers, single consumer.
 *
 * @code
 *  lockless_queue<int> s;
 *  s.produce(1);
 *  s.produce(2);
 *  s.produce(3);
 *  for (auto h: lockless_iterable(s))
 *  {
 *      std::cout << h << std::endl;
 *  }
 * @endcode
 */
template<typename T>
class lockless_queue
{
public:
    template<typename NodeDataType>
    struct node
    {
        // Enables people to access data type for this node externally.
        typedef NodeDataType DataType;
        node(const DataType& data)
          : data(data), next(nullptr) {}
        DataType data;
        node* next;
    };

    typedef node<T> NodeType;

    lockless_queue()
      : lock_(false), head_(nullptr) {}

    void produce(const T &data)
    {
        NodeType* new_node = new NodeType(data);
        while (lock_.exchange(true)) {}
        new_node->next = head_;
        head_ = new_node;
        lock_ = false;
    }
  
    NodeType* consume_all()
    {
        while (lock_.exchange(true)) {}
        NodeType* ret_node = head_;
        head_ = nullptr;
        lock_ = false;
        return ret_node;
    }
private:
    std::atomic<bool> lock_;
    NodeType* head_;
};

// Classes to allow iteration over the consumed items.
// These mutate as they're iterated, so they only support
// basic iteration from beginning to end.

/**
 * Iterator for lockless queue which deletes consumed items
 * as they iterated over.
 * Once iteration is complete, memory should be cleared.
 */
template <typename NodeType>
class lockless_iterator
{
public:
    lockless_iterator(NodeType* ptr)
      : ptr_(ptr) {}

    template <typename LockLessIterator>
    bool operator!=(const LockLessIterator& other)
    {
        return ptr_ != other.ptr_;
    }

    typename NodeType::DataType operator*() const
    {
        return ptr_->data;
    }

    void operator++()
    {
        NodeType* tmp = ptr_->next;
        delete ptr_;
        ptr_ = tmp;
    }
private:
    NodeType* ptr_;
};

template <typename LockLessQueue>
class lockless_iterable_dispatch
{
public:
    typedef typename LockLessQueue::NodeType NodeType;

    lockless_iterable_dispatch(LockLessQueue& queue)
      : head_(queue.consume_all()) {}

    lockless_iterator<NodeType> begin()
    {
        return lockless_iterator<NodeType>(head_);
    }

    lockless_iterator<NodeType> end()
    {
        return lockless_iterator<NodeType>(nullptr);
    }
private:
    NodeType* head_;
};

// This is needed so that when creating the iterable object,
// we don't need to specify the template arguments.
// Functions are able to deduce the template args from the param list.
template <typename LockLessQueue>
lockless_iterable_dispatch<
    LockLessQueue
>
lockless_iterable(LockLessQueue& queue)
{
    return lockless_iterable_dispatch<LockLessQueue>(queue);
}

} // namespace obelisk

#endif

