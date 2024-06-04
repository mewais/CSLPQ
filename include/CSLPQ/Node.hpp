#ifndef __CSLPQ_NODE_HPP__
#define __CSLPQ_NODE_HPP__

#include <vector>

#include "Concepts.hpp"
#include "Pointers.hpp"

namespace CSLPQ
{
    template<typename K>
    class Node
    {
        static_assert(is_comparable<K>::value, "Key type must be totally ordered");
        public:
            typedef jss::shared_ptr<Node<K>> SPtr;
            typedef jss::atomic_shared_ptr<Node<K>> ASPtr;
            typedef jss::markable_atomic_shared_ptr<Node<K>> MASPtr;

        private:
            K priority;
            int level;
            std::vector<MASPtr> next;
            std::atomic<bool> inserting;

        public:
            Node(const K& priority, int level) : priority(priority), level(level), next(level), inserting(true)
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

            bool IsInserting() const
            {
                return this->inserting.load();
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
                return this->next[level].compare_exchange_weak(old_value, new_value);
            }

            void SetDoneInserting()
            {
                this->inserting.store(false);
            }
    };

    template<typename K, typename V>
    class KVNode
    {
        static_assert(is_comparable<K>::value, "Key type must be totally ordered");
        static_assert(std::is_move_constructible<V>::value || std::is_copy_constructible<V>::value ||
                      std::is_default_constructible<V>::value || std::is_fundamental<V>::value, 
                      "Value type must be fundamental, or default constructible, or copy or move constructible");
        public:
            typedef jss::shared_ptr<KVNode<K, V>> SPtr;
            typedef jss::atomic_shared_ptr<KVNode<K, V>> ASPtr;
            typedef jss::markable_atomic_shared_ptr<KVNode<K, V>> MASPtr;

        private:
            K priority;
            V data;
            int level;
            std::vector<MASPtr> next;
            std::atomic<bool> inserting;

        public:
            template <typename T = V>
            KVNode(const K& priority, int level, 
                   typename std::enable_if<std::is_default_constructible<T>::value, int>::type = 0) : priority(priority),
                   data(V()), level(level), next(level), inserting(true)
            {
            }

            template <typename T = V>
            KVNode(const K& priority, const V& value, int level, 
                   typename std::enable_if<std::is_fundamental<T>::value, int>::type = 0) : priority(priority), 
                   data(value), level(level), next(level), inserting(true)
            {
            }

            template <typename T = V>
            KVNode(const K& priority, const V& value, int level,
                   typename std::enable_if<std::is_move_constructible<T>::value && !std::is_fundamental<T>::value, int>::type = 0) : 
                   priority(priority), data(std::move(value)), level(level), next(level), inserting(true)
            {
            }

            template <typename T = V>
            KVNode(const K& priority, const V& value, int level,
                   typename std::enable_if<std::is_copy_constructible<T>::value && !std::is_move_constructible<T>::value, int>::type = 0) : 
                   priority(priority), data(value), level(level), next(level), inserting(true)
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
                return this->inserting.load();
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
                return this->next[level].compare_exchange_weak(old_value, new_value);
            }

            void SetDoneInserting()
            {
                this->inserting.store(false);
            }
    };
}

#endif // __CSLPQ_NODE_HPP__
