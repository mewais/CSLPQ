#ifndef __CSLPQ_NODE_HPP__
#define __CSLPQ_NODE_HPP__

#include <vector>

#include "Concepts.hpp"
#include "Pointers.hpp"

namespace CSLPQ
{
    template<KeyType K, ValueType V>
    class Node
    {
        public:
            typedef jss::shared_ptr<Node<K, V>> SPtr;
            typedef jss::atomic_shared_ptr<Node<K, V>> ASPtr;
            typedef jss::markable_atomic_shared_ptr<Node<K, V>> MASPtr;

        private:
            K priority;
            V data;
            int level;
            std::vector<MASPtr> next;
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

            SPtr GetNextPointer(int level) const
            {
                return this->next[level].load();
            }

            bool IsNextMarked(int level) const
            {
                return this->next[level].is_marked();
            }

            std::pair<SPtr , bool> GetNextPointerAndMark(int level) const
            {
                return this->next[level].load_marked();
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

            void SetNext(int level, SPtr node)
            {
                this->next[level] = node;
            }

            void SetNextMark(int level)
            {
                this->next[level].set_mark();
            }

            bool TestAndSetMark(int level, SPtr& expected)
            {
                return this->next[level].test_and_set_mark(expected);
            }

            bool CompareExchange(int level, SPtr& old_value, SPtr new_value)
            {
                return this->next[level].compare_exchange_strong(old_value, new_value);
            }

            void SetDoneInserting()
            {
                this->inserting.clear();
            }
    };
}

#endif // __CSLPQ_NODE_HPP__