#ifndef ORDERBOOK_MEMORYPOOL_H
#define ORDERBOOK_MEMORYPOOL_H

#include <vector>
#include <stack>
#include <stdexcept>

template <typename T>

class MemoryPool {
    private:
        std::vector<T> pool;            // actual storage - contiguous
        std::stack<size_t> free_list;   // stack of free indexes in pool

    public:
        explicit MemoryPool(size_t capacity) {
            pool.resize(capacity);
            for (size_t i = 0; i < capacity; i++) {
                free_list.push(i);
            }
        }

        T* allocate() {
            if (free_list.empty()) {
                throw std::runtime_error("MemoryPool exhausted!");
            }
            size_t idx = free_list.top();
            free_list.pop();
            return &pool[idx];
        }

        void deallocate(T* ptr) {
            size_t idx = ptr - &pool[0];
            free_list.push(idx);
        }
};
#endif // ORDERBOOK_MEMORYPOOL_H