#include <atomic>
#include <vector>

template <typename K, typename V>
class Node
{
    public:
        K key;
        V value;
        std::vector<std::atomic<Node<K, V>*>> next;

        Node(int level, K key, V value)
            : key(key), value(value), next(level + 1)
        {
            for (int i = 0; i <= level; i++)
            {
                next[i].store(nullptr);
            }
        }
};

template <typename K, typename V>
class SkipList
{
    public:
        SkipList(int maxLevel)
            : head(new Node<K, V>(maxLevel, K(), V())), level(0)
        {
            for (int i = 0; i <= maxLevel; i++)
            {
                head->next[i].store(nullptr);
            }
        }

        void insert(K key, V value)
        {
            std::vector<Node<K, V>*> update(head->next.size());
            Node<K, V> *curr = head;

            for (int i = level; i >= 0; i--)
            {
                while (curr->next[i].load() != nullptr &&
                       curr->next[i].load()->key < key)
                {
                    curr = curr->next[i].load();
                }
                update[i] = curr;
            }

            Node<K, V> *nextNode = curr->next[0].load();
            if (nextNode == nullptr || nextNode->key != key)
            {
                int newLevel = randomLevel();
                if (newLevel > level)
                {
                    for (int i = level + 1; i <= newLevel; i++)
                    {
                        update[i] = head;
                    }
                    level = newLevel;
                }

                Node<K, V> *newNode = new Node<K, V>(newLevel, key, value);
                for (int i = 0; i <= newLevel; i++)
                {
                    newNode->next[i].store(update[i]->next[i].load());
                    update[i]->next[i].store(newNode);
                }
            }
        }

        V pop(K key)
        {
            std::vector<Node<K, V>*> update(head->next.size());
            Node<K, V> *curr = head;

            for (int i = level; i >= 0; i--)
            {
                while (curr->next[i].load() != nullptr &&
                       curr->next[i].load()->key < key)
                {
                    curr = curr->next[i].load();
                }
                update[i] = curr;
            }

            Node<K, V> *nextNode = curr->next[0].load();
            if (nextNode != nullptr && nextNode->key == key)
            {
                for (int i = 0; i <= level; i++)
                {
                    if (update[i]->next[i].load() != nextNode)
                    {
                        break;
                    }
                    update[i]->next[i].store(nextNode->next[i].load());
                }
                while (level > 0 && head->next[level].load() == nullptr)
                {
                    level--;
                }
                V value = nextNode->value;
                delete nextNode;
                return value;
            }
            return V();
        }

        int randomLevel()
        {
            int lvl = 0;
            while (((double)rand() / RAND_MAX < 0.5) && lvl < head->next.size() - 1)
            {
                lvl++;
            }
            return lvl;
        }

    private:
        Node<K, V> *head;
        int level;
};
