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

            const NodeMPtr& GetNext(int level) const
            {
                return this->next[level];
            }

            Node<K, V>* GetNextPointer(int level) const
            {
                return this->next[level].GetPointer();
            }

            bool IsNextMarked(int level) const
            {
                return this->next[level].IsMarked();
            }

            std::pair<Node<K, V>*, bool> GetNextPointerAndMark(int level) const
            {
                return this->next[level].GetPointerAndMark();
            }

            int GetLevel() const
            {
                return this->level;
            }

            K GetPriority() const
            {
                return this->priority;
            }

            V GetData() const
            {
                return this->data;
            }

            void SetNext(int level, Node<K, V>* node)
            {
                this->next[level] = node;
            }

            void SetNext(int level, NodeMPtr node)
            {
                this->next[level] = node;
            }

            void SetNextMark(int level)
            {
                this->next[level].SetMark();
            }

            bool CompareExchange(int level, Node<K, V>* old_value, bool old_mark, Node<K, V>* new_value, bool new_mark)
            {
                return this->next[level].CompareExchange(old_value, old_mark, new_value, new_mark);
            }
    };
}

#endif // __CSLPQ_NODE_HPP__