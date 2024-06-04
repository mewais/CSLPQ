#include <iostream>
#include <thread>
#include <pthread.h>
#include <vector>
#include <set>
#include <algorithm>

#include "CSLPQ/Queue.hpp"

#define COUNT 100000

CSLPQ::KVQueue<uint64_t, uint64_t> queue;
std::vector<std::vector<std::pair<uint64_t, uint64_t>>> keys;
std::set<std::pair<uint64_t, uint64_t>> keys_ref;
pthread_barrier_t barrier;

void insert(std::vector<std::pair<uint64_t, uint64_t>>& local_keys)
{
    pthread_barrier_wait(&barrier);
    for (uint64_t i = 0; i < COUNT / 10; i++)
    {
        queue.Push(local_keys[i].first, local_keys[i].second);
    }
}

void remove()
{
    pthread_barrier_wait(&barrier);
    uint64_t count = 0;
    while (!keys_ref.empty())
    {
        uint64_t key;
        uint64_t value;
        std::pair<uint64_t, uint64_t> key_value;
        if (queue.TryPop(key, value))
        {
            key_value = std::make_pair(key, value);
            if (keys_ref.find(std::make_pair(key, value)) == keys_ref.end())
            {
                std::cerr << "FAILURE-" << count << ": Read " << key << ": " << value << " which has already been removed" << std::endl;
                return;
            }
            else
            {
                // std::cout << "SUCCESS-" << count << ": Key " << key << " read successfully" << std::endl;
                keys_ref.erase(std::make_pair(key, value));
            }
            count++;
        }
    }
}

int main()
{
    pthread_barrier_init(&barrier, NULL, 10);

    // First, fill the keys and ref
    uint64_t count = 0;
    std::vector<std::pair<uint64_t, uint64_t>> full_keys;
    for (uint64_t i = 0; i < COUNT / 1000; i++)
    {
        for (uint64_t j = 0; j < 1000; j++)
        {
            full_keys.emplace_back(j, count);
            keys_ref.insert(std::make_pair(j, count++));
        }
    }

    // Shuffle the keys
    std::random_shuffle(full_keys.begin(), full_keys.end());

    // Split among threads
    keys.resize(10);
    for (uint64_t i = 0; i < 10; i++)
    {
        keys[i] = std::vector<std::pair<uint64_t, uint64_t>>(full_keys.begin() + i * COUNT / 10, full_keys.begin() + (i + 1) * COUNT / 10);
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
