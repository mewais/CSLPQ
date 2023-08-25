#include <iostream>
#include <thread>
#include <barrier>
#include <vector>
#include <set>
#include <algorithm>

#include "CSLPQ/Queue.hpp"

#define COUNT 100000

CSLPQ::KVQueue<uint64_t, void*> queue;
std::vector<std::vector<uint64_t>> keys;
std::set<uint64_t> keys_ref;
std::barrier barrier(11);

void insert(std::vector<uint64_t>& local_keys)
{
    barrier.arrive_and_wait();
    for (uint64_t i = 0; i < COUNT / 10; i++)
    {
        queue.Push(local_keys[i], (void*)i);
    }
}

void remove()
{
    barrier.arrive_and_wait();
    uint64_t count = 0;
    while (true)
    {
        uint64_t key;
        void* value;
        if (queue.TryPop(key, value))
        {
            count++;
            if (keys_ref.find(key) == keys_ref.end())
            {
                std::cerr << "FAILURE-" << count << ": Read " << key << ": " << value << " which has already been removed" << std::endl;
                return;
            }
            else
            {
                // std::cout << "SUCCESS-" << count << ": Key " << key << " read successfully" << std::endl;
                keys_ref.erase(key);
            }
            if (count == COUNT)
            {
                return;
            }
        }
    }
}

int main()
{
    // First, fill the keys and ref
    std::vector<uint64_t> full_keys;
    for (uint64_t i = 0; i < COUNT; i++)
    {
        full_keys.emplace_back(i);
        keys_ref.insert(i);
    }

    // Shuffle the keys
    std::random_shuffle(full_keys.begin(), full_keys.end());

    // Split among threads
    keys.resize(10);
    for (uint64_t i = 0; i < 10; i++)
    {
        keys[i] = std::vector<uint64_t>(full_keys.begin() + i * COUNT / 10, full_keys.begin() + (i + 1) * COUNT / 10);
    }

    // Start the threads
    std::cout << "Starting threads" << std::endl;
    std::vector<std::thread> ts;
    for (uint64_t i = 0; i < 10; i++)
    {
        ts.emplace_back(insert, std::ref(keys[i]));
    }
    remove();
    for (uint64_t i = 0; i < 10; i++)
    {
        ts[i].join();
    }

    return 0;
}