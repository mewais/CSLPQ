#include <iostream>
#include <vector>
#include <random>
#include <atomic>
#include <cassert>

const int MAX_LEVEL = 32;

template<typename T>
class Node
{
public:
    T data;
    int priority;
    std::vector<std::atomic<Node<T>*>> next;
    Node(T data, int priority, int level) : data(data), priority(priority), next(level + 1)
    {
        for (int i = 0; i < level + 1; i++)
        {
            next[i].store(nullptr);
        }
    }
};

class RandomNumberGenerator
{
public:
    RandomNumberGenerator() : mt(rd()), dist(0, MAX_LEVEL - 1) {}
    int operator()() { return dist(mt); }
private:
    std::random_device rd;
    std::mt19937 mt;
    std::uniform_int_distribution<int> dist;
};

template<typename T>
class LockFreePriorityQueue
{
public:
    LockFreePriorityQueue() : head(new Node<T>(T(), 0, MAX_LEVEL)), level(0) {}
    ~LockFreePriorityQueue()
    {
        while (head.load() != nullptr)
        {
            Node<T>* curr = head.load();
            head.store(curr->next[0].load());
            delete curr;
        }
    }

    void push(T data, int priority)
    {
        int newLevel = randomNumberGenerator();
        Node<T>* newNode = new Node<T>(data, priority, newLevel);
        Node<T>* curr = head.load();
        std::vector<Node<T>*> pred(MAX_LEVEL + 1);
        for (int i = level; i >= 0; i--)
        {
            while (curr->next[i].load() != nullptr && curr->next[i].load()->priority < priority)
            {
                curr = curr->next[i].load();
            }
            pred[i] = curr;
        }
        curr = curr->next[0].load();
        for (int i = 0; i <= newLevel; i++)
        {
            newNode->next[i].store(pred[i]->next[i].load());
            pred[i]->next[i].store(newNode);
        }
        while (level < newLevel && head.load()->next[level + 1].load() == nullptr)
        {
            level++;
        }
    }

    T pop()
    {
        Node<T>* curr = head.load();
        while (true)
        {
            if (curr->next[0].load() == nullptr)
            {
                return T();
            }
            Node<T>* nextNode = curr->next[0].load();
            if (head.compare_exchange_weak(curr, nextNode))
            {
                T data = nextNode->data;
                delete nextNode;
                while (level > 0 && head.load()->next[level].load() == nullptr)
                {
                    level--;
                }
                return data;
            }
            curr = curr->next[0].load();
        }
    }

private:
    std::atomic<Node<T>*> head;
    int level;
    RandomNumberGenerator randomNumberGenerator;
};
