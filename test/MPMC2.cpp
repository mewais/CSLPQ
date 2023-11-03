#include <iostream>
#include <thread>
#include <barrier>
#include <vector>
#include <set>
#include <algorithm>

#include "CSLPQ/Queue.hpp"

#define COUNT 100000

CSLPQ::Queue<uint64_t> queue;
std::vector<std::vector<uint64_t>> keys;
std::set<uint64_t> keys_ref;
std::mutex keys_ref_mutex;
std::barrier barrier(10);
std::atomic<uint64_t> count = 0;

void insert(std::vector<uint64_t>& local_keys)
{
    barrier.arrive_and_wait();
    for (uint64_t i = 0; i < COUNT / 10; i++)
    {
        queue.Push(local_keys[i]);
    }
}

void remove_()
{
    while (count != COUNT)
    {
        uint64_t key;
        if (queue.TryPop(key))
        {
            count++;
            keys_ref_mutex.lock();
            if (keys_ref.find(key) == keys_ref.end())
            {
                keys_ref_mutex.unlock();
                std::cerr << "FAILURE: Read " << key << " which has already been removed" << std::endl;
                return;
            }
            else
            {
                // std::cout << "SUCCESS: Key " << key << " read successfully" << std::endl;
                keys_ref.erase(key);
                keys_ref_mutex.unlock();
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
        ts.emplace_back(remove_);
    }
    for (uint64_t i = 0; i < 10; i++)
    {
        ts.emplace_back(insert, std::ref(keys[i]));
    }
    for (uint64_t i = 0; i < 20; i++)
    {
        ts[i].join();
    }

    return 0;
}