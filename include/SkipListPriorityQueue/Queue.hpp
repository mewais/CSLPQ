#ifndef __SLPQ_QUEUE_HPP__
#define __SLPQ_QUEUE_HPP__

#include <vector>
#include <random>
#include <shared_mutex>

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
            std::shared_mutex mutex;
            
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
                std::vector<Node<K, V>*> predecessors(new_level, nullptr);
                Node<K, V>* current = this->head;

                // Find predecessors
                this->mutex.lock_shared();
                for (int i = new_level - 1; i >= 0; i--)
                {
                    while (current->GetNext(i) != nullptr && current->GetNext(i)->GetPriority() < priority)
                    {
                        Node<K, V>* tmp = current->GetNext(i);
                        current = tmp;
                    }
                    predecessors[i] = current;
                }
                this->mutex.unlock_shared();

                // Insert new node
                this->mutex.lock();
                for (int i = 0; i < new_level; i++)
                {
                    new_node->GetNext()[i] = predecessors[i]->GetNext(i);
                    predecessors[i]->GetNext()[i] = new_node;
                }
                this->mutex.unlock();
            }

            void Push(const K& priority, const V& data)
            {
                int new_level = this->GenerateRandomLevel();
                auto new_node = new Node<K, V>(priority, data, new_level);
                std::vector<Node<K, V>*> predecessors(new_level, nullptr);
                Node<K, V>* current = this->head;

                // Find predecessors
                this->mutex.lock_shared();
                for (int i = new_level - 1; i >= 0; i--)
                {
                    while (current->GetNext(i) != nullptr && current->GetNext(i)->GetPriority() < priority)
                    {
                        Node<K, V>* tmp = current->GetNext(i);
                        current = tmp;
                    }
                    predecessors[i] = current;
                }
                this->mutex.unlock_shared();

                // Insert new node
                this->mutex.lock();
                for (int i = 0; i < new_level; i++)
                {
                    new_node->GetNext()[i] = predecessors[i]->GetNext(i);
                    predecessors[i]->GetNext()[i] = new_node;
                }
                this->mutex.unlock();
            }

            bool TryPop(K& priority, V& data)
            {
                this->mutex.lock();
                Node<K, V>* current = this->head->GetNext(0);

                if (current == nullptr)
                {
                    this->mutex.unlock();
                    return false;
                }
                else
                {
                    priority = current->GetPriority();
                    data = current->GetData();
                    Node<K, V>* tmp = current->GetNext(0);

                    this->head->GetNext()[0] = tmp;
                    delete current;
                    this->mutex.unlock();
                    return true;
                }
            }
    };
}

#endif // __SLPQ_QUEUE_HPP__