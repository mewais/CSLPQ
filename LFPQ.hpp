#include <vector>
#include <random>
#include <mutex>

template<typename K, typename V>
class SLPQNode
{
    private:
        K priority;
        V data;
        std::vector<SLPQNode<K, V>*> next;
        std::mutex mutex;

    public:
        SLPQNode(K& priority, V& data, int level) : priority(priority), data(data), next(level, nullptr)
        {
        }

        std::vector<SLPQNode<K, V>*>& GetNext()
        {
            return this->next;
        }

        SLPQNode<K, V>* GetNext(int i)
        {
            return this->next[i];
        }

        K GetPriority()
        {
            return this->priority;
        }

        V GetData()
        {
            return this->data;
        }

        void Lock()
        {
            this->mutex.lock();
        }

        void Unlock()
        {
            this->mutex.unlock();
        }
};

template<typename K, typename V>
class SLPQ
{
    private:
        int max_level;
        SLPQNode<K, V>* head;
        
        int GenerateRandomLevel()
        {
            static std::random_device rd;
            static std::mt19937 mt(rd());
            static std::uniform_int_distribution<int> dist(0, this->max_level - 1);

            return dist(mt);
        }

    public:
        SLPQ(int max_level = 32) : max_level(max_level), head(new SLPQNode<K, V>(K(), V(), max_level)) {}
        
        ~SLPQ()
        {
            
        }

        void Push(K priority, V data)
        {
            SLPQNode<K, V>* current = this->head;
            std::vector<SLPQNode<K, V>*> predecessors(this->max_level, nullptr);
            for (int i = this->max_level - 1; i >= 0; i--)
            {
                current->Lock();
                while (current->GetNext(i) != nullptr && current->GetNext(i)->GetPriority() < priority)
                {
                    SLPQNode<K, V>* tmp = current->GetNext(i);
                    current->Unlock();
                    current = tmp;
                    current->Lock();
                }
                current->Unlock();
                predecessors[i] = current;
            }

            int new_level = this->GenerateRandomLevel();
            SLPQNode<K, V>* new_node = new SLPQNode<K, V>(priority, data, new_level);
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
            SLPQNode<K, V>* current = this->head->GetNext(0);
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
                SLPQNode<K, V>* tmp = current->GetNext(0);
                current->Unlock();

                this->head->Lock();
                this->head->GetNext()[0] = tmp;
                this->head.Unlock();
                delete current;
                return true;
            }
        }
};