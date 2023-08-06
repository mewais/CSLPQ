#include <iostream>

#include "CSLPQ/Queue.hpp"

int main()
{
    CSLPQ::Queue<std::pair<uint64_t, uint64_t>, void*> queue;
    queue.Push(std::make_pair(11, 2), (void*)5);
    queue.Push(std::make_pair(10, 2), (void*)1);
    queue.Push(std::make_pair(12, 1), (void*)8);
    queue.Push(std::make_pair(13, 3), (void*)15);
    queue.Push(std::make_pair(12, 4), (void*)11);
    queue.Push(std::make_pair(14, 1), (void*)16);
    queue.Push(std::make_pair(12, 3), (void*)10);
    queue.Push(std::make_pair(11, 3), (void*)6);
    queue.Push(std::make_pair(10, 3), (void*)2);
    queue.Push(std::make_pair(15, 4), (void*)23);
    queue.Push(std::make_pair(14, 2), (void*)17);
    queue.Push(std::make_pair(11, 1), (void*)4);
    queue.Push(std::make_pair(15, 3), (void*)22);
    queue.Push(std::make_pair(14, 3), (void*)18);
    queue.Push(std::make_pair(12, 5), (void*)12);
    queue.Push(std::make_pair(10, 1));
    queue.Push(std::make_pair(15, 2), (void*)21);
    queue.Push(std::make_pair(15, 1), (void*)20);
    queue.Push(std::make_pair(12, 2), (void*)9);
    queue.Push(std::make_pair(11, 4), (void*)7);
    queue.Push(std::make_pair(13, 1), (void*)13);
    queue.Push(std::make_pair(10, 4), (void*)3);
    queue.Push(std::make_pair(13, 2), (void*)14);
    queue.Push(std::make_pair(14, 4), (void*)19);

    while (true)
    {
        std::pair<uint64_t, uint64_t> key;
        void* value;
        if (queue.TryPop(key, value))
        {
            std::cout << key.first << ", " << key.second << ": " << value << std::endl;
        }
        else
        {
            break;
        }
    }

    return 0;
}