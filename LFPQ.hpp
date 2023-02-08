#include <vector>
#include <atomic>
#include <random>

template <typename K, typename V>
class SkipListNode
{
    private:
        K key;
        V value;
        std::vector<std::atomic<SkipListNode<K, V>*>> next;

    public:
        SkipListNode(int level, K key, V value)
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
class SkipList
{
    private:
        const int maxLevel = 16;
        std::atomic<int> level;
        SkipListNode<K, V> *head;

    public:
        SkipList()
        {
            this->level = 0;
            head = new SkipListNode<K, V>(level, K(), V());
        }

        void Push(K key, V value)
        {
            std::vector<std::atomic<SkipListNode<K, V>*>> update(this->head->next.size());
            SkipListNode<K, V> *current = this->head;
            for (int i = this->level; i >= 0; i--)
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
                int lvl = randomLevel();
                if (lvl > this->level)
                {
                    for (int i = this->level + 1; i <= lvl; i++)
                    {
                        update[i] = this->head;
                    }
                    this->level = lvl;
                }
                SkipListNode<K, V> *newNode = new SkipListNode<K, V>(lvl, key, value);
                for (int i = 0; i <= lvl; i++)
                {
                    newNode->next[i].store(update[i]->next[i].load());
                    update[i]->next[i].store(newNode);
                }
            }
        }

        V Pop()
        {
            SkipListNode<K, V> *current = this->head->next[0].load();
            if (current)
            {
                std::vector<std::atomic<SkipListNode<K, V>*>> update(this->head->next.size());
                for (int i = this->level; i >= 0; i--)
                {
                    while (current->next[i].load() && current->next[i].load()->key)
                    {
                        current = current->next[i].load();
                    }
                    update[i] = current;
                }
                current = current->next[0].load();
                for (int i = 0; i <= this->level; i++)
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

        int randomLevel()
        {
            static std::mt19937 eng(std::random_device{}());
            static std::uniform_int_distribution<int> dist(0, 1);

            int lvl = 0;
            while (dist(eng) == 0 && lvl < maxLevel)
            {
                lvl++;
            }
            return lvl;
        }
};