#ifndef __SLPQ_NODE_HPP__
#define __SLPQ_NODE_HPP__

#include <vector>

#include "Concepts.hpp"

namespace SLPQ
{
    template<KeyType K, ValueType V>
    class Node
    {
        private:
            K priority;
            V data;
            std::vector<Node<K, V>*> next;

        public:
            Node(const K& priority, int level) requires std::is_default_constructible_v<V> : priority(priority), data(V()),
            next(level, nullptr)
            {
            }

            Node(const K& priority, const V& value, int level) requires OnlyMoveConstructible<V> : priority(priority),
            data(std::move(value)), next(level, nullptr)
            {
            }

            Node(const K& priority, const V& value, int level) requires OnlyCopyConstructible<V> : priority(priority),
            data(value), next(level, nullptr)
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
    };
}

#endif // __SLPQ_NODE_HPP__