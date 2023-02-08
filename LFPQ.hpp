#include <iostream>
#include <vector>
#include <atomic>

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
    public:
        SkipList()
        {
            level = 0;
            head = new SkipListNode<K, V>(level, K(), V());
        }

        void Push(K key, V value)
        {
            std::vector<std::atomic<SkipListNode<K, V> *>> update(head->next.size());
            SkipListNode<K, V> *current = head;
            for (int i = level; i >= 0; i--)
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
                if (lvl > level)
                {
                    for (int i = level + 1; i <= lvl; i++)
                    {
                        update[i] = head;
                    }
                    level = lvl;
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
            SkipListNode<K, V> *current = head;
            SkipListNode<K, V> *update[maxLevel + 1];
            memset(update, 0, sizeof(SkipListNode<K, V> *) * (maxLevel + 1));
            for (int i = level; i >= 0; i--)
            {
                while (current->next[i].load() && current->next[i].load()->key <= key)
                {
                    current = current->next[i].load();
                }
                update[i] = current;
            }

            current = current->next[0].load();
            if (current)
            {
                SkipListNode<K, V> *nextNode = current->next[0].load();
                for (int i = 0; i <= level; i++)
                {
                    if (update[i]->next[i].load() != current)
                    {
                        break;
                    }
                    update[i]->next[i].store(nextNode);
                }
                V value = current->value;
                delete current;
                while (level > 0 && head->next[level].load() == nullptr)
                {
                    level--;
                }
                return value;
            }
            return V();
        }

        int randomLevel()
        {
            int lvl = 0;
            while (rand() < RAND_MAX / 2 && lvl < maxLevel)
            {
                lvl++;
            }
            return lvl;
        }

        const int maxLevel = 16;
        int level;
        SkipListNode<K, V> *head;
};
