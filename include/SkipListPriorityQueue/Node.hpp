#ifndef __SLPQ_NODE_HPP__
#define __SLPQ_NODE_HPP__

#include <vector>
#include <mutex>

namespace SLPQ
{
    template<typename K, typename V>
    class Node
    {
        private:
            K priority;
            V data;
            std::vector<Node<K, V>*> next;
            std::mutex mutex;

        public:
            Node(K& priority, V& data, int level) : priority(priority), data(data), next(level, nullptr)
            {
            }

            std::vector<Node<K, V>*>& GetNext()
            {
                return this->next;
            }

            Node<K, V>* GetNext(int i)
            {
                return this->next[i];
            }

            K GetPriority()
            {
                return this->priority;
            }

            V GetData()
            {
                return this->data;
            }

            void Lock()
            {
                this->mutex.lock();
            }

            void Unlock()
            {
                this->mutex.unlock();
            }
    };
}

#endif // __SLPQ_NODE_HPP__