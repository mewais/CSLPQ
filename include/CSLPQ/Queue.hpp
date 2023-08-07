#ifndef __CSLPQ_QUEUE_HPP__
#define __CSLPQ_QUEUE_HPP__

#include <vector>
#include <random>
#include <tuple>
#include <shared_mutex>
#include <sstream>

#include "Concepts.hpp"
#include "Node.hpp"

namespace CSLPQ
{
    template<KeyType K, ValueType V>
    class Queue
    {
        private:
            const int max_level;
            Node<K, V>* head;

            int GenerateRandomLevel()
            {
                static std::random_device rd;
                static std::mt19937 mt(rd());
                static std::uniform_int_distribution<int> dist(1, this->max_level + 1);

                return dist(mt);
            }

            void FindLastOfPriority(const K& priority, std::vector<Node<K, V>*>& predecessors,
                                    std::vector<Node<K, V>*>& successors)
            {
                bool marked = false;
                bool snip = false;

                Node<K, V>* predecessor = nullptr;
                Node<K, V>* current = nullptr;
                Node<K, V>* successor = nullptr;

                bool retry;
                while (true)
                {
                    retry = false;
                    predecessor = this->head;
                    for (auto level = this->max_level; level >= 0; --level)
                    {
                        current = predecessor->GetNext(level).GetPointer();
                        while (current)
                        {
                            std::tie(successor, marked) = current->GetNext(level).GetPointerAndMark();
                            while (marked)
                            {
                                snip = predecessor->GetNext(level).CompareExchange(current, false, successor, false);
                                if (!snip)
                                {
                                    retry = true;
                                    break;
                                }
                                current = successor;
                                if (current == nullptr)
                                {
                                    marked = false;
                                }
                                else
                                {
                                    std::tie(successor, marked) = current->GetNext(level).GetPointerAndMark();
                                }
                            }
                            if (retry)
                            {
                                break;
                            }
                            if (current == nullptr)
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

            Node<K, V>* FindFirst()
            {
                bool marked = false;
                bool snip = false;

                Node<K, V>* predecessor = nullptr;
                Node<K, V>* current = nullptr;
                Node<K, V>* successor = nullptr;

                bool retry;
                while (true)
                {
                    retry = false;
                    predecessor = this->head;
                    for (auto level = this->max_level; level >= 0; --level)
                    {
                        current = predecessor->GetNext(level).GetPointer();
                        if (current)
                        {
                            std::tie(successor, marked) = current->GetNext(level).GetPointerAndMark();
                            while (marked)
                            {
                                snip = predecessor->GetNext(level).CompareExchange(current, false, successor, false);
                                if (!snip)
                                {
                                    retry = true;
                                    break;
                                }
                                current = successor;
                                if (current == nullptr)
                                {
                                    marked = false;
                                }
                                else
                                {
                                    std::tie(successor, marked) = current->GetNext(level).GetPointerAndMark();
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
                            return nullptr;
                        }
                    }
                }
            }

        public:
            Queue(int max_level = 4) : max_level(max_level)
            {
                this->head = new Node<K, V>(K(), max_level + 1);
            }
            
            ~Queue()
            {
                Node<K, V>* current = this->head;
                while (current != nullptr)
                {
                    Node<K, V>* tmp = current->GetNext(0).GetPointer();
                    delete current;
                    current = tmp;
                }
            }

            void Push(const K& priority)
            {
                int new_level = this->GenerateRandomLevel();
                auto new_node = new Node<K, V>(priority, new_level);
                std::vector<Node<K, V>*> predecessors(this->max_level + 1, nullptr);
                std::vector<Node<K, V>*> successors(this->max_level + 1, nullptr);

                while (true)
                {
                    this->FindLastOfPriority(priority, predecessors, successors);
                    for (auto level = 0; level < new_level; ++level)
                    {
                        new_node->SetNext(level, successors[level]);
                    }
                    auto predecessor = predecessors[0];
                    auto successor = successors[0];
                    if (!predecessor->GetNext(0).CompareExchange(successor, false, new_node, false))
                    {
                        continue;
                    }
                    for (auto level = 1; level < new_level; ++level)
                    {
                        while (true)
                        {
                            predecessor = predecessors[level];
                            successor = successors[level];
                            if (predecessor->GetNext(level).CompareExchange(successor, false, new_node, false))
                            {
                                break;
                            }
                            this->FindLastOfPriority(priority, predecessors, successors);
                        }
                    }
                    break;
                }
            }

            void Push(const K& priority, const V& data)
            {
                int new_level = this->GenerateRandomLevel();
                auto new_node = new Node<K, V>(priority, data, new_level);
                std::vector<Node<K, V>*> predecessors(this->max_level + 1, nullptr);
                std::vector<Node<K, V>*> successors(this->max_level + 1, nullptr);

                while (true)
                {
                    this->FindLastOfPriority(priority, predecessors, successors);
                    for (auto level = 0; level < new_level; ++level)
                    {
                        new_node->SetNext(level, successors[level]);
                    }
                    auto predecessor = predecessors[0];
                    auto successor = successors[0];
                    if (!predecessor->GetNext(0).CompareExchange(successor, false, new_node, false))
                    {
                        continue;
                    }
                    for (auto level = 1; level < new_level; ++level)
                    {
                        while (true)
                        {
                            predecessor = predecessors[level];
                            successor = successors[level];
                            if (predecessor->GetNext(level).CompareExchange(successor, false, new_node, false))
                            {
                                break;
                            }
                            this->FindLastOfPriority(priority, predecessors, successors);
                        }
                    }
                    break;
                }
            }

            bool TryPop(K& priority, V& data)
            {
                Node<K, V>* successor = nullptr;
                Node<K, V>* first = this->FindFirst();

                if (first == nullptr)
                {
                    return false;
                }

                bool marked = false;
                for (int level = first->GetLevel() - 1; level >= 1; --level)
                {
                    first->GetNext(level).SetMark();
                }

                marked = false;
                std::tie(successor, marked) = first->GetNext(0).GetPointerAndMark();
                while (true)
                {
                    bool success = first->GetNext(0).CompareExchange(successor, false, successor, true);
                    std::tie(successor, marked) = first->GetNext(0).GetPointerAndMark();
                    if (success)
                    {
                        priority = first->GetPriority();
                        data = first->GetData();
                        return true;
                    }
                    else if (marked)
                    {
                        return false;
                    }
                }
            }

            std::string ToString(bool all_levels = false) requires Printable<K> && Printable<V>
            {
                std::stringstream ss;
                int max = all_levels ? this->max_level : 0;
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
                    Node<K, V>* node;
                    Node<K, V>* nnode;
                    std::tie(node, marked) = this->head->GetNext(level).GetPointerAndMark();
                    while (node)
                    {
                        std::tie(nnode, marked) = node->GetNext(level).GetPointerAndMark();
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