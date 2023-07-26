#ifndef __SLPQ_QUEUE_HPP__
#define __SLPQ_QUEUE_HPP__

#include <vector>
#include <random>
#include <mutex>

#include "Concepts.hpp"
#include "Node.hpp"

namespace SLPQ
{
    template<KeyType K, ValueType V>
    class Queue
    {
        private:
            int max_level;
            Node<K, V>* head;
            
            int GenerateRandomLevel()
            {
                static std::random_device rd;
                static std::mt19937 mt(rd());
                static std::uniform_int_distribution<int> dist(1, this->max_level);

                return dist(mt);
            }

        public:
            Queue(int max_level = 32) : max_level(max_level), head(new Node<K, V>(K(), max_level)) {}
            
            ~Queue()
            {
                Node<K, V>* current = this->head;
                while (current != nullptr)
                {
                    Node<K, V>* tmp = current->GetNext(0);
                    delete current;
                    current = tmp;
                }
            }

            void Push(const K& priority)
            {
                int new_level = this->GenerateRandomLevel();
                auto new_node = new Node<K, V>(priority, new_level);

                Node<K, V>* current = this->head;
                std::vector<Node<K, V>*> predecessors(new_level, nullptr);
                for (int i = new_level - 1; i >= 0; i--)
                {
                    current->Lock();
                    while (current->GetNext(i) != nullptr && current->GetNext(i)->GetPriority() < priority)
                    {
                        Node<K, V>* tmp = current->GetNext(i);
                        current->Unlock();
                        current = tmp;
                        current->Lock();
                    }
                    current->Unlock();
                    predecessors[i] = current;
                }

                for (int i = 0; i < new_level; i++)
                {
                    predecessors[i]->Lock();
                    new_node->GetNext()[i] = predecessors[i]->GetNext(i);
                    predecessors[i]->GetNext()[i] = new_node;
                    predecessors[i]->Unlock();
                }
            }

            void Push(const K& priority, const V& data)
            {
                int new_level = this->GenerateRandomLevel();
                auto new_node = new Node<K, V>(priority, data, new_level);

                Node<K, V>* current = this->head;
                std::vector<Node<K, V>*> predecessors(new_level, nullptr);
                for (int i = new_level - 1; i >= 0; i--)
                {
                    current->Lock();
                    while (current->GetNext(i) != nullptr && current->GetNext(i)->GetPriority() < priority)
                    {
                        Node<K, V>* tmp = current->GetNext(i);
                        current->Unlock();
                        current = tmp;
                        current->Lock();
                    }
                    current->Unlock();
                    predecessors[i] = current;
                }

                for (int i = 0; i < new_level; i++)
                {
                    predecessors[i]->Lock();
                    new_node->GetNext()[i] = predecessors[i]->GetNext(i);
                    predecessors[i]->GetNext()[i] = new_node;
                    predecessors[i]->Unlock();
                }
            }

            bool TryPop(K& priority, V& data)
            {
                this->head->Lock();
                Node<K, V>* current = this->head->GetNext(0);
                this->head->Unlock();

                if (current == nullptr)
                {
                    return false;
                }
                else
                {
                    current->Lock();
                    priority = current->GetPriority();
                    data = current->GetData();
                    Node<K, V>* tmp = current->GetNext(0);
                    current->Unlock();

                    this->head->Lock();
                    this->head->GetNext()[0] = tmp;
                    this->head->Unlock();
                    delete current;
                    return true;
                }
            }
    };
}

#endif // __SLPQ_QUEUE_HPP__