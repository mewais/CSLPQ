#ifndef __SLPQ_QUEUE_HPP__
#define __SLPQ_QUEUE_HPP__

#include <vector>
#include <random>
#include <mutex>
#include <concepts>

#include "Node.hpp"

namespace SLPQ
{
    template<typename K, typename V>
    requires std::totally_ordered<K>
    class Queue
    {
        private:
            int max_level;
            Node<K, V>* head;
            
            int GenerateRandomLevel()
            {
                static std::random_device rd;
                static std::mt19937 mt(rd());
                static std::uniform_int_distribution<int> dist(0, this->max_level - 1);

                return dist(mt);
            }

        public:
            Queue(int max_level = 32) : max_level(max_level), head(new Node<K, V>(K(), V(), max_level)) {}
            
            ~Queue()
            {
                Node<K, V> *current = head->GetNext(0), *tmp;
                while (current != nullptr)
                {
                    tmp = current->GetNext(0);
                    delete current;
                    current = tmp;
                }
                delete head;
            }

            void Push(K priority, V data)
            {
                Node<K, V>* current = this->head;
                std::vector<Node<K, V>*> predecessors(this->max_level, nullptr);
                for (int i = this->max_level - 1; i >= 0; i--)
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

                int new_level = this->GenerateRandomLevel();
                Node<K, V>* new_node = new Node<K, V>(priority, data, new_level);
                for (int i = 0; i <= new_level; i++)
                {
                    predecessors[i]->Lock();
                    new_node->next[i] = predecessors[i]->next[i];
                    predecessors[i]->next[i] = new_node;
                    predecessors[i]->Unlock();
                }
            }

            bool TryPop(K& priority, V& data)
            {
                this->head.Lock();
                Node<K, V>* current = this->head->GetNext(0);
                this->head.Unlock();

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
                    this->head.Unlock();
                    delete current;
                    return true;
                }
            }
    };
}

#endif // __SLPQ_QUEUE_HPP__