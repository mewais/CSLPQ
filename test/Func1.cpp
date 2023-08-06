#include <iostream>

#include "CSLPQ/Queue.hpp"

int main()
{
    CSLPQ::Queue<uint64_t, void*> queue(8);
    while (true)
    {
        uint64_t key;
        void* value;
        if (queue.TryPop(key, value))
        {
            std::cerr << "FAILURE: Read " << key << ": " << value << " from empty queue" << std::endl;
            return 1;
        }
        else
        {
            break;
        }
    }

    queue.Push(112, (void*)5);
    std::cout << "First " << queue.ToString();
    queue.Push(102, (void*)1);
    std::cout << "Second " << queue.ToString();
    queue.Push(121, (void*)8);
    std::cout << "Third " << queue.ToString();
    queue.Push(133, (void*)15);
    std::cout << "Fourth " << queue.ToString();
    queue.Push(124, (void*)11);
    queue.Push(141, (void*)16);
    queue.Push(123, (void*)10);
    queue.Push(113, (void*)6);
    queue.Push(103, (void*)2);
    queue.Push(154, (void*)23);
    queue.Push(142, (void*)17);
    queue.Push(111, (void*)4);
    queue.Push(153, (void*)22);
    queue.Push(143, (void*)18);
    queue.Push(125, (void*)12);
    queue.Push(101);
    queue.Push(152, (void*)21);
    queue.Push(151, (void*)20);
    queue.Push(122, (void*)9);
    queue.Push(114, (void*)7);
    queue.Push(131, (void*)13);
    queue.Push(104, (void*)3);
    queue.Push(101, (void*)0x10000);
    queue.Push(132, (void*)14);
    queue.Push(144, (void*)19);
    std::cout << "End " << queue.ToString() << "\n";

    while (true)
    {
        uint64_t key;
        void* value;
        if (queue.TryPop(key, value))
        {
            std::cout << key << ": " << value << std::endl;
        }
        else
        {
            break;
        }
    }

    return 0;
}