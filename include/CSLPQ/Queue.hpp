#ifndef __CSLPQ_QUEUE_HPP__
#define __CSLPQ_QUEUE_HPP__

#include <vector>
#include <random>
#include <tuple>
#include <sstream>

#include "Concepts.hpp"
#include "Node.hpp"

namespace CSLPQ
{
    template<KeyType K>
    class Queue
    {
        private:
            typedef jss::shared_ptr<Node<K>> SPtr;

            const uint32_t max_level;
            const uint32_t max_size;
            SPtr head;
            std::atomic<uint32_t> count;

            void Wait()
            {
                if (this->max_size)
                {
                    while (this->count >= this->max_size);
                }
            }

            uint32_t GenerateRandomLevel()
            {
                static std::random_device rd;
                static std::mt19937 mt(rd());
                static std::uniform_int_distribution<uint32_t> dist(1, this->max_level + 1);

                return dist(mt);
            }

            void FindLastOfPriority(const K& priority, std::vector<SPtr>& predecessors,
                                    std::vector<SPtr>& successors)
            {
                bool marked = false;
                bool snip = false;

                SPtr predecessor;
                SPtr current;
                SPtr successor;

                bool retry;
                while (true)
                {
                    retry = false;
                    predecessor = this->head;
                    for (int64_t level = this->max_level; level >= 0; --level)
                    {
                        current = predecessor->GetNextPointer(level);
                        while (current)
                        {
                            std::tie(successor, marked) = current->GetNextPointerAndMark(level);
                            while (marked)
                            {
                                snip = predecessor->CompareExchange(level, current, successor);
                                if (!snip)
                                {
                                    retry = true;
                                    break;
                                }
                                current = successor;
                                if (!current)
                                {
                                    marked = false;
                                }
                                else
                                {
                                    std::tie(successor, marked) = current->GetNextPointerAndMark(level);
                                }
                            }
                            if (retry)
                            {
                                break;
                            }
                            if (!current)
                            {
                                break;
                            }
                            if (current->GetPriority() < priority)
                            {
                                predecessor = current;
                                current = successor;
                            }
                            else
                            {
                                break;
                            }
                        }
                        if (retry)
                        {
                            break;
                        }
                        predecessors[level] = predecessor;
                        successors[level] = current;
                    }
                    if (!retry)
                    {
                        break;
                    }
                }
            }

            SPtr FindFirst()
            {
                bool marked = false;
                bool snip = false;

                SPtr predecessor;
                SPtr current;
                SPtr successor;
                SPtr empty;

                bool retry;
                while (true)
                {
                    retry = false;
                    predecessor = this->head;
                    for (int64_t level = this->max_level; level >= 0; --level)
                    {
                        current = predecessor->GetNextPointer(level);
                        if (current)
                        {
                            std::tie(successor, marked) = current->GetNextPointerAndMark(level);
                            while (marked)
                            {
                                snip = predecessor->CompareExchange(level, current, successor);
                                if (!snip)
                                {
                                    retry = true;
                                    break;
                                }
                                current = successor;
                                if (!current)
                                {
                                    marked = false;
                                }
                                else
                                {
                                    std::tie(successor, marked) = current->GetNextPointerAndMark(level);
                                }
                            }
                            if (retry)
                            {
                                break;
                            }
                            if (level == 0)
                            {
                                return current;
                            }
                        }
                        else if (level == 0)
                        {
                            return empty;
                        }
                    }
                }
            }

        public:
            explicit Queue(uint32_t max_level = 4, uint32_t max_size = 0) : max_level(max_level), max_size(max_size),
                           head(new Node<K>(K(), max_level + 1)), count(0)
            {
            }

            Queue(const Queue&) = delete;

            Queue(Queue&& other) noexcept : max_level(other.max_level), max_size(other.max_size), head(other.head),
                  count(other.count)
            {
                other.head = nullptr;
            }

            Queue& operator=(const Queue&) = delete;

            Queue& operator=(Queue&& other) noexcept
            {
                this->max_level = other.max_level;
                this->max_size = other.max_size;
                this->head = other.head;
                this->count = other.count;
                other.head = nullptr;
                return *this;
            }

            void Push(const K& priority)
            {
                this->Wait();
                uint32_t new_level = this->GenerateRandomLevel();
                SPtr new_node(new Node<K>(priority, new_level));
                std::vector<SPtr> predecessors(this->max_level + 1);
                std::vector<SPtr> successors(this->max_level + 1);

                while (true)
                {
                    this->FindLastOfPriority(priority, predecessors, successors);
                    for (uint32_t level = 0; level < new_level; ++level)
                    {
                        new_node->SetNext(level, successors[level]);
                    }
                    if (!predecessors[0]->CompareExchange(0, successors[0], new_node))
                    {
                        continue;
                    }
                    for (uint32_t level = 1; level < new_level; ++level)
                    {
                        while (true)
                        {
                            if (predecessors[level]->CompareExchange(level, successors[level], new_node))
                            {
                                break;
                            }
                            this->FindLastOfPriority(priority, predecessors, successors);
                        }
                    }
                    break;
                }
                new_node->SetDoneInserting();
                this->count++;
            }

            bool TryPop(K& priority)
            {
                SPtr successor;
                SPtr first = this->FindFirst();

                if (!first)
                {
                    return false;
                }
                if (first->IsInserting())
                {
                    return false;
                }

                for (uint32_t level = first->GetLevel() - 1; level >= 1; --level)
                {
                    first->SetNextMark(level);
                }

                successor = first->GetNextPointer(0);
                priority = first->GetPriority();
                bool success = first->TestAndSetMark(0, successor);
                if (success)
                {
                    this->count--;
                    return true;
                }
                else
                {
                    return false;
                }
            }

            uint32_t GetCount() const
            {
                return this->count.load();
            }

            std::string ToString(bool all_levels = false) requires Printable<K>
            {
                std::stringstream ss;
                uint32_t max = all_levels? this->max_level : 0;
                for (uint32_t level = 0; level <= max; ++level)
                {
                    if (all_levels)
                    {
                        ss << "Queue at level " << level << ":\n";
                    }
                    else
                    {
                        ss << "Queue: \n";
                    }

                    bool marked = false;
                    SPtr node;
                    SPtr nnode;
                    std::tie(node, marked) = this->head->GetNextPointerAndMark(level);
                    while (node)
                    {
                        std::tie(nnode, marked) = node->GetNextPointerAndMark(level);
                        if (!marked)
                        {
                            ss << "\tKey: " << node->GetPriority() << "\n";
                        }
                        else
                        {
                            ss << "\tKey: " << node->GetPriority() << " (Marked)\n";
                        }
                        node = nnode;
                    }
                }
                return ss.str();
            }
    };

    template<KeyType K, ValueType V>
    class KVQueue
    {
        private:
            typedef jss::shared_ptr<KVNode<K, V>> SPtr;

            const uint32_t max_level;
            const uint32_t max_size;
            SPtr head;
            std::atomic<uint32_t> count;

            void Wait()
            {
                if (this->max_size)
                {
                    while (this->count >= this->max_size);
                }
            }

            uint32_t GenerateRandomLevel()
            {
                static std::random_device rd;
                static std::mt19937 mt(rd());
                static std::uniform_int_distribution<uint32_t> dist(1, this->max_level + 1);

                return dist(mt);
            }

            void FindLastOfPriority(const K& priority, std::vector<SPtr>& predecessors,
                                    std::vector<SPtr>& successors)
            {
                bool marked = false;
                bool snip = false;

                SPtr predecessor;
                SPtr current;
                SPtr successor;

                bool retry;
                while (true)
                {
                    retry = false;
                    predecessor = this->head;
                    for (int64_t level = this->max_level; level >= 0; --level)
                    {
                        current = predecessor->GetNextPointer(level);
                        while (current)
                        {
                            std::tie(successor, marked) = current->GetNextPointerAndMark(level);
                            while (marked)
                            {
                                snip = predecessor->CompareExchange(level, current, successor);
                                if (!snip)
                                {
                                    retry = true;
                                    break;
                                }
                                current = successor;
                                if (!current)
                                {
                                    marked = false;
                                }
                                else
                                {
                                    std::tie(successor, marked) = current->GetNextPointerAndMark(level);
                                }
                            }
                            if (retry)
                            {
                                break;
                            }
                            if (!current)
                            {
                                break;
                            }
                            if (current->GetPriority() < priority)
                            {
                                predecessor = current;
                                current = successor;
                            }
                            else
                            {
                                break;
                            }
                        }
                        if (retry)
                        {
                            break;
                        }
                        predecessors[level] = predecessor;
                        successors[level] = current;
                    }
                    if (!retry)
                    {
                        break;
                    }
                }
            }

            SPtr FindFirst()
            {
                bool marked = false;
                bool snip = false;

                SPtr predecessor;
                SPtr current;
                SPtr successor;
                SPtr empty;

                bool retry;
                while (true)
                {
                    retry = false;
                    predecessor = this->head;
                    for (int64_t level = this->max_level; level >= 0; --level)
                    {
                        current = predecessor->GetNextPointer(level);
                        if (current)
                        {
                            std::tie(successor, marked) = current->GetNextPointerAndMark(level);
                            while (marked)
                            {
                                snip = predecessor->CompareExchange(level, current, successor);
                                if (!snip)
                                {
                                    retry = true;
                                    break;
                                }
                                current = successor;
                                if (!current)
                                {
                                    marked = false;
                                }
                                else
                                {
                                    std::tie(successor, marked) = current->GetNextPointerAndMark(level);
                                }
                            }
                            if (retry)
                            {
                                break;
                            }
                            if (level == 0)
                            {
                                return current;
                            }
                        }
                        else if (level == 0)
                        {
                            return empty;
                        }
                    }
                }
            }

        public:
            KVQueue(uint32_t max_level = 4, uint32_t max_size = 0) : max_level(max_level), max_size(max_size),
                    head(new KVNode<K, V>(K(), max_level + 1)), count(0)
            {
            }

            KVQueue(const KVQueue&) = delete;

            KVQueue(KVQueue&& other)  noexcept : max_level(other.max_level), max_size(other.max_size), head(other.head),
                    count(other.count)
            {
                other.head = nullptr;
                other.count = 0;
            }

            KVQueue& operator=(const KVQueue&) = delete;

            KVQueue& operator=(KVQueue&& other) noexcept
            {
                this->max_level = other.max_level;
                this->max_size = other.max_size;
                this->head = other.head;
                this->count = other.count;
                other.head = nullptr;
                other.count = 0;
                return *this;
            }

            void Push(const K& priority)
            {
                this->Wait();
                uint32_t new_level = this->GenerateRandomLevel();
                SPtr new_node(new KVNode<K, V>(priority, new_level));
                std::vector<SPtr> predecessors(this->max_level + 1);
                std::vector<SPtr> successors(this->max_level + 1);

                while (true)
                {
                    this->FindLastOfPriority(priority, predecessors, successors);
                    for (uint32_t level = 0; level < new_level; ++level)
                    {
                        new_node->SetNext(level, successors[level]);
                    }
                    if (!predecessors[0]->CompareExchange(0, successors[0], new_node))
                    {
                        continue;
                    }
                    for (uint32_t level = 1; level < new_level; ++level)
                    {
                        while (true)
                        {
                            if (predecessors[level]->CompareExchange(level, successors[level], new_node))
                            {
                                break;
                            }
                            this->FindLastOfPriority(priority, predecessors, successors);
                        }
                    }
                    break;
                }
                new_node->SetDoneInserting();
                this->count++;
            }

            void Push(const K& priority, const V& data)
            {
                this->Wait();
                uint32_t new_level = this->GenerateRandomLevel();
                SPtr new_node(new KVNode<K, V>(priority, data, new_level));
                std::vector<SPtr> predecessors(this->max_level + 1);
                std::vector<SPtr> successors(this->max_level + 1);

                while (true)
                {
                    this->FindLastOfPriority(priority, predecessors, successors);
                    for (uint32_t level = 0; level < new_level; ++level)
                    {
                        new_node->SetNext(level, successors[level]);
                    }
                    if (!predecessors[0]->CompareExchange(0, successors[0], new_node))
                    {
                        continue;
                    }
                    for (uint32_t level = 1; level < new_level; ++level)
                    {
                        while (true)
                        {
                            if (predecessors[level]->CompareExchange(level, successors[level], new_node))
                            {
                                break;
                            }
                            this->FindLastOfPriority(priority, predecessors, successors);
                        }
                    }
                    break;
                }
                new_node->SetDoneInserting();
                this->count++;
            }

            bool TryPop(K& priority, V& data)
            {
                SPtr successor;
                SPtr first = this->FindFirst();

                if (!first)
                {
                    return false;
                }
                if (first->IsInserting())
                {
                    return false;
                }

                for (uint32_t level = first->GetLevel() - 1; level >= 1; --level)
                {
                    first->SetNextMark(level);
                }

                successor = first->GetNextPointer(0);
                priority = first->GetPriority();
                data = first->GetData();
                bool success = first->TestAndSetMark(0, successor);
                if (success)
                {
                    this->count--;
                    return true;
                }
                else
                {
                    return false;
                }
            }

            uint32_t GetCount() const
            {
                return this->count.load();
            }

            std::string ToString(bool all_levels = false) requires Printable<K> && Printable<V>
            {
                std::stringstream ss;
                uint32_t max = all_levels? this->max_level : 0;
                for (uint32_t level = 0; level <= max; ++level)
                {
                    if (all_levels)
                    {
                        ss << "Queue at level " << level << ":\n";
                    }
                    else
                    {
                        ss << "Queue: \n";
                    }

                    bool marked = false;
                    SPtr node;
                    SPtr nnode;
                    std::tie(node, marked) = this->head->GetNextPointerAndMark(level);
                    while (node)
                    {
                        std::tie(nnode, marked) = node->GetNextPointerAndMark(level);
                        if (!marked)
                        {
                            ss << "\tKey: " << node->GetPriority() << ", Value: " << node->GetData() << "\n";
                        }
                        else
                        {
                            ss << "\tKey: " << node->GetPriority() << ", Value: " << node->GetData() << " (Marked)\n";
                        }
                        node = nnode;
                    }
                }
                return ss.str();
            }
    };
}

#endif // __CSLPQ_QUEUE_HPP__