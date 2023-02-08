#include <vector>
#include <atomic>
#include <random>

template <typename K, typename V>
class LFPQNode
{
    private:
        K key;
        V value;
        std::vector<std::atomic<LFPQNode<K, V>*>> next;

    public:
        LFPQNode(int level, K key, V value)
            : key(key), value(value), next(level + 1)
        {
            for (int i = 0; i <= level; i++)
            {
                this->next[i].store(nullptr);
            }
        }

        K GetKey()
        {
            return this->key;
        }

        V GetValue()
        {
            return this->value;
        }
};

template <typename K, typename V>
class LFPQ
{
    private:
        const int max_level = 16;
        std::atomic<int> level;
        LFPQNode<K, V> *head;

    public:
        LFPQ()
        {
            this->level = 0;
            head = new LFPQNode<K, V>(level, K(), V());
        }

        int RandomLevel()
        {
            static std::mt19937 eng(std::random_device{}());
            static std::uniform_int_distribution<int> dist(0, this->max_level - 1);

            return dist(eng);
        }

        void Push(K key, V value)
        {
            std::vector<std::atomic<LFPQNode<K, V>*>> update(this->head->next.size());
            LFPQNode<K, V> *current = this->head;
            for (int i = this->level.load(); i >= 0; i--)
            {
                while (current->next[i].load() && current->next[i].load()->key < key)
                {
                    current = current->next[i].load();
                }
                update[i] = current;
            }
            current = current->next[0].load();
            if (current == nullptr || current->key != key)
            {
                int lvl = RandomLevel();
                if (lvl > this->level.load())
                {
                    for (int i = this->level.load() + 1; i <= lvl; i++)
                    {
                        update[i] = this->head;
                    }
                    this->level = lvl;
                }
                LFPQNode<K, V> *newNode = new LFPQNode<K, V>(lvl, key, value);
                for (int i = 0; i <= lvl; i++)
                {
                    newNode->next[i].store(update[i]->next[i].load());
                    update[i]->next[i].store(newNode);
                }
            }
        }

        V Pop()
        {
            LFPQNode<K, V> *current = this->head->next[0].load();
            if (current)
            {
                std::vector<std::atomic<LFPQNode<K, V>*>> update(this->head->next.size());
                for (int i = this->level.load(); i >= 0; i--)
                {
                    while (current->next[i].load() && current->next[i].load()->key)
                    {
                        current = current->next[i].load();
                    }
                    update[i] = current;
                }
                current = current->next[0].load();
                for (int i = 0; i <= this->level.load(); i++)
                {
                    update[i]->next[i].store(current->next[i].load());
                }
                V value = current->value;
                delete current;
                while (this->level > 0 && this->head->next[this->level].load() == nullptr)
                {
                    this->level--;
                }
                return value;
            }
            return V();
        }
};