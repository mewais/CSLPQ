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
    template<KeyType K, ValueType V>
    class Queue
    {
        private:
            typedef SharedPointer<Node<K, V>> NodePtr;

            const int max_level;
            NodePtr head;

            int GenerateRandomLevel()
            {
                static std::random_device rd;
                static std::mt19937 mt(rd());
                static std::uniform_int_distribution<int> dist(1, this->max_level + 1);

                return dist(mt);
            }

            void FindLastOfPriority(const K& priority, std::vector<NodePtr>& predecessors,
                                    std::vector<NodePtr>& successors)
            {
                bool marked = false;
                bool snip = false;

                NodePtr predecessor;
                NodePtr current;
                NodePtr successor;

                bool retry;
                while (true)
                {
                    retry = false;
                    predecessor = this->head;
                    for (auto level = this->max_level; level >= 0; --level)
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

            NodePtr FindFirst()
            {
                bool marked = false;
                bool snip = false;

                NodePtr predecessor;
                NodePtr current;
                NodePtr successor;
                NodePtr empty;

                bool retry;
                while (true)
                {
                    retry = false;
                    predecessor = this->head;
                    for (auto level = this->max_level; level >= 0; --level)
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
            Queue(int max_level = 4) : max_level(max_level), head(new Node<K, V>(K(), max_level + 1))
            {
            }

            void Push(const K& priority)
            {
                int new_level = this->GenerateRandomLevel();
                NodePtr new_node(new Node<K, V>(priority, new_level));
                std::vector<NodePtr> predecessors(this->max_level + 1);
                std::vector<NodePtr> successors(this->max_level + 1);

                while (true)
                {
                    this->FindLastOfPriority(priority, predecessors, successors);
                    for (auto level = 0; level < new_level; ++level)
                    {
                        new_node->SetNext(level, successors[level]);
                    }
                    if (!predecessors[0]->CompareExchange(0, successors[0], new_node))
                    {
                        continue;
                    }
                    for (auto level = 1; level < new_level; ++level)
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
            }

            void Push(const K& priority, const V& data)
            {
                int new_level = this->GenerateRandomLevel();
                NodePtr new_node(new Node<K, V>(priority, data, new_level));
                std::vector<NodePtr> predecessors(this->max_level + 1);
                std::vector<NodePtr> successors(this->max_level + 1);

                while (true)
                {
                    this->FindLastOfPriority(priority, predecessors, successors);
                    for (auto level = 0; level < new_level; ++level)
                    {
                        new_node->SetNext(level, successors[level]);
                    }
                    if (!predecessors[0]->CompareExchange(0, successors[0], new_node))
                    {
                        continue;
                    }
                    for (auto level = 1; level < new_level; ++level)
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
            }

            bool TryPop(K& priority, V& data)
            {
                NodePtr successor;
                NodePtr first = this->FindFirst();

                if (!first)
                {
                    return false;
                }
                if (first->IsInserting())
                {
                    return false;
                }

                for (int level = first->GetLevel() - 1; level >= 1; --level)
                {
                    first->SetNextMark(level);
                }

                successor = first->GetNextPointer(0);
                priority = first->GetPriority();
                data = first->GetData();
                bool success = first->TestAndSetMark(0, successor);
                if (success)
                {
                    return true;
                }
                else
                {
                    return false;
                }
            }

            std::string ToString(bool all_levels = false) requires Printable<K> && Printable<V>
            {
                std::stringstream ss;
                int max = all_levels? this->max_level : 0;
                for (int level = 0; level <= max; ++level)
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
                    NodePtr node;
                    NodePtr nnode;
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