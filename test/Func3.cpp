#include <iostream>
#include <vector>
#include <set>
#include <algorithm>

#include "CSLPQ/Queue.hpp"

#define COUNT 10000

CSLPQ::Queue<uint64_t, void*> queue;
std::vector<uint64_t> keys;
std::set<uint64_t> keys_ref;

void insert()
{
    for (uint64_t i = 0; i < COUNT; i++)
    {
        queue.Push(keys[i], (void*)i);
    }
}

void remove()
{
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
                std::cerr << "FAILURE-" << count << ": Read " << key << ": " << value << " which have already been removed" << std::endl;
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
    for (uint64_t i = 0; i < COUNT; i++)
    {
        keys.emplace_back(i);
        keys_ref.insert(i);
    }

    // Shuffle the keys
    std::random_shuffle(keys.begin(), keys.end());

    // Start the threads
    insert();
    remove();

    return 0;
}