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
            typedef SharedPointer<Node<K, V>> NodePtr;
            typedef MarkedSharedPointer<Node<K, V>> MNodePtr;

        private:
            K priority;
            V data;
            int level;
            std::vector<MNodePtr> next;
            std::atomic_flag inserting;

        public:
            Node(const K& priority, int level) requires std::is_default_constructible_v<V> : priority(priority),
                 data(V()), level(level), next(level), inserting(true)
            {
            }

            Node(const K& priority, const V& value, int level) requires std::is_fundamental_v<V> : priority(priority),
                 data(value), level(level), next(level), inserting(true)
            {
            }

            Node(const K& priority, const V& value, int level) requires OnlyMoveConstructible<V> : priority(priority),
                 data(std::move(value)), level(level), next(level), inserting(true)
            {
            }

            Node(const K& priority, const V& value, int level) requires OnlyCopyConstructible<V> : priority(priority),
                 data(value), level(level), next(level), inserting(true)
            {
            }

            const MNodePtr& GetNext(int level) const
            {
                return this->next[level];
            }

            NodePtr GetNextPointer(int level) const
            {
                return this->next[level].GetPointer();
            }

            bool IsNextMarked(int level) const
            {
                return this->next[level].IsMarked();
            }

            std::pair<NodePtr , bool> GetNextPointerAndMark(int level) const
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

            bool IsInserting() const
            {
                return this->inserting.test();
            }

            void SetNext(int level, NodePtr node)
            {
                this->next[level] = node;
            }

            void SetNext(int level, MNodePtr node)
            {
                this->next[level] = node;
            }

            void SetNextMark(int level)
            {
                this->next[level].SetMark();
            }

            bool TestAndSetMark(int level, NodePtr& expected)
            {
                return this->next[level].TestAndSetMark(expected);
            }

            bool CompareExchange(int level, NodePtr& old_value, NodePtr & new_value)
            {
                return this->next[level].CompareExchange(old_value, new_value);
            }

            void SetDoneInserting()
            {
                this->inserting.clear();
            }
    };
}

#endif // __CSLPQ_NODE_HPP__