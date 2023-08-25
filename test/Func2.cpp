#include <iostream>

#include "CSLPQ/Queue.hpp"

int main()
{
    CSLPQ::Queue<std::pair<uint64_t, uint64_t>> queue;
    queue.Push(std::make_pair(11, 2));
    queue.Push(std::make_pair(10, 2));
    queue.Push(std::make_pair(12, 1));
    queue.Push(std::make_pair(13, 3));
    queue.Push(std::make_pair(12, 4));
    queue.Push(std::make_pair(14, 1));
    queue.Push(std::make_pair(12, 3));
    queue.Push(std::make_pair(11, 3));
    queue.Push(std::make_pair(10, 3));
    queue.Push(std::make_pair(15, 4));
    queue.Push(std::make_pair(14, 2));
    queue.Push(std::make_pair(11, 1));
    queue.Push(std::make_pair(15, 3));
    queue.Push(std::make_pair(14, 3));
    queue.Push(std::make_pair(12, 5));
    queue.Push(std::make_pair(10, 1));
    queue.Push(std::make_pair(15, 2));
    queue.Push(std::make_pair(15, 1));
    queue.Push(std::make_pair(12, 2));
    queue.Push(std::make_pair(11, 4));
    queue.Push(std::make_pair(13, 1));
    queue.Push(std::make_pair(10, 4));
    queue.Push(std::make_pair(13, 2));
    queue.Push(std::make_pair(14, 4));

    while (true)
    {
        std::pair<uint64_t, uint64_t> key;
        if (queue.TryPop(key))
        {
            std::cout << key.first << ", " << key.second << std::endl;
        }
        else
        {
            break;
        }
    }

    return 0;
}