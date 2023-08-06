#ifndef __CSLPQ_NODE_HPP__
#define __CSLPQ_NODE_HPP__

#include <vector>

#include "Concepts.hpp"
#include "MarkedPointer.hpp"

namespace CSLPQ
{
    template<KeyType K, ValueType V>
    class Node
    {
        public:
            typedef MarkedPointer<Node<K, V>> NodeMPtr;

        private:
            K priority;
            V data;
            int level;
            std::vector<NodeMPtr> next;

        public:
            Node(const K& priority, int level) requires std::is_default_constructible_v<V> : priority(priority),
            data(V()), level(level), next(level)
            {
            }

            Node(const K& priority, const V& value, int level) requires std::is_fundamental_v<V> : priority(priority),
            data(value), level(level), next(level)
            {
            }

            Node(const K& priority, const V& value, int level) requires OnlyMoveConstructible<V> : priority(priority),
            data(std::move(value)), level(level), next(level)
            {
            }

            Node(const K& priority, const V& value, int level) requires OnlyCopyConstructible<V> : priority(priority),
            data(value), level(level), next(level)
            {
            }

            void SetNext(int level, Node<K, V>* node)
            {
                this->next[level] = node;
            }

            void SetNext(int level, NodeMPtr node)
            {
                this->next[level] = node;
            }

            void SetNext(std::vector<NodeMPtr> nodes)
            {
                this->next = nodes;
            }

            std::vector<NodeMPtr>& GetNext()
            {
                return this->next;
            }

            NodeMPtr& GetNext(int level)
            {
                return this->next[level];
            }

            int GetLevel()
            {
                return this->level;
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

#endif // __CSLPQ_NODE_HPP__