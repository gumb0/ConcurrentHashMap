#ifndef CONCURRENT_HASH_MAP_H
#define CONCURRENT_HASH_MAP_H

#include <algorithm>
#include <atomic>
#include <mutex>
#include <utility>


class ConcurrentHashmapException
{
public:
    enum
    {
        InvalidCapacity,
        InvalidConcurrencyLevel,
        KeyNotFound
    };

    explicit ConcurrentHashmapException(int code) : mCode(code) {}

    int getCode() const
    {
        return mCode;
    }

private:
    int mCode;
};


template<class Key, class Value, class Hash = std::hash<Key>>
class ConcurrentHashmap
{
    static const std::size_t ConcurrencyLevelDefault = 16;

    struct Node
    {
        const Key key;
        Value value;
        Node* next;
    };

    class NodeList;

public:
    typedef std::pair<const Value&, std::unique_lock<std::mutex>> LockedValue;

    explicit ConcurrentHashmap(
        std::size_t capacity, 
        std::size_t concurrencyLevel = ConcurrencyLevelDefault, 
        const Hash& hasher = Hash()) : 
        mCapacity(capacity),
        mMutexCount(getMutexCount(capacity, concurrencyLevel)),
        mIndicesPerMutex(getIndicesPerMutex(mCapacity, mMutexCount)),
        mHasher(hasher),
        mSize(0),
        mTable(new NodeList[capacity]),
        mMutexes(new std::mutex[mMutexCount])
    {
    }

    ~ConcurrentHashmap()
    {
        delete[] mMutexes;
        delete[] mTable;
    }

    // Reserved size of hash table
    std::size_t capacity() const
    {
        return mCapacity;
    }
        
    // Actual number of stored keys
    std::size_t size() const
    {
        return mSize;
    }

    // In multithreaded environment true result does not guarantee that key still exists in the map after return from find.
    bool find(const Key& key) const
    {
        const std::size_t index = getIndex(key);
        std::lock_guard<std::mutex> lock(getMutex(index));

        return mTable[index].find(key) != nullptr;
    }

    // Returns copy of value stored in the map or throws ConcurrentHashmapException if the key is not found.
    // In multithreaded environment it's not guaranteed that key still exists in the map after return from getCopy.
    Value getCopy(const Key& key) const
    {
        const std::size_t index = getIndex(key);
        std::lock_guard<std::mutex> lock(getMutex(index));

        if (const Node* node = mTable[index].find(key))
            return node->value;
        else
            throw ConcurrentHashmapException(ConcurrentHashmapException::KeyNotFound);

    }

    // Returns a reference to the value stored in the map paired with the lock.
    // The value is garanteed to exist in the map as long as the lock is locked.
    LockedValue get(const Key& key) const
    {
        const std::size_t index = getIndex(key);
        std::unique_lock<std::mutex> lock(getMutex(index));

        if (const Node* node = mTable[index].find(key))
            return LockedValue(node->value, std::move(lock));
        else
            throw ConcurrentHashmapException(ConcurrentHashmapException::KeyNotFound);
    }

    // Inserts new key-value into the map or overwrires the old value if the key already existed.
    void insert(const Key& key, const Value& value)
    {
        const std::size_t index = getIndex(key);
        std::lock_guard<std::mutex> lock(getMutex(index));

        if (mTable[index].insert(key, value))
            ++mSize;
    }

    // Deletes key from the map or does nothing if key is not found
    void erase(const Key& key)
    {
        const std::size_t index = getIndex(key);
        std::lock_guard<std::mutex> lock(getMutex(index));

        if (mTable[index].erase(key))
            --mSize;
    }

private:
    // noncopyable
    ConcurrentHashmap(const ConcurrentHashmap&) = delete;
    ConcurrentHashmap& operator=(const ConcurrentHashmap&) = delete;

    std::size_t getMutexCount(std::size_t capacity, std::size_t concurrencyLevel) const
    {
        if (capacity == 0)
            throw ConcurrentHashmapException(ConcurrentHashmapException::InvalidCapacity);
        if (concurrencyLevel == 0)
            throw ConcurrentHashmapException(ConcurrentHashmapException::InvalidConcurrencyLevel);

        return std::min(concurrencyLevel, capacity);
    }
    
    std::size_t getIndicesPerMutex(std::size_t capacity, std::size_t mutexCount) const
    {
        if (capacity % mutexCount)
            return (capacity + mutexCount) / mutexCount; // ceil(capacity / mutexCount)
        else
            return capacity / mutexCount;
    }

    std::size_t getIndex(const Key& key) const
    {
        return mHasher(key) % mCapacity;
    }

    std::mutex& getMutex(std::size_t tableIndex) const
    {
        const std::size_t mutexIndex = tableIndex / mIndicesPerMutex;
        return mMutexes[mutexIndex];
    }

private:
    const std::size_t mCapacity;
    const std::size_t mMutexCount;
    const std::size_t mIndicesPerMutex;
    const Hash mHasher;
    std::atomic<std::size_t> mSize;
    NodeList* mTable;
    std::mutex* mMutexes;
};

template<class Key, class Value, class Hash>
class ConcurrentHashmap<Key, Value, Hash>::NodeList
{
public:
    NodeList() : mHead(nullptr) {}
    ~NodeList()
    {
        while (mHead)
            deleteHead();
    }

    Node* find(const Key& key) const
    {
        Node* node = mHead;
        while (node && node->key != key)
            node = node->next;

        return node;
    }

    // Returns true if inserted, false if key already existed and value was overwirtten.
    bool insert(const Key& key, const Value& value)
    {
        if (Node* node = find(key))
        {
            node->value = value;
            return false;
        }

        Node* newNode = new Node{ key, value, mHead };
        mHead = newNode;
        return true;
    }

    // Returns true if deleted, false if key not found.
    bool erase(const Key& key)
    {
        if (!mHead)
            return false;

        if (mHead->key == key)
        {
            deleteHead();
            return true;
        }

        Node* prev = mHead;
        Node* node = mHead->next;
        while (node && node->key != key)
        {
            prev = node;
            node = node->next;
        }

        if (node)
        {
            prev->next = node->next;
            delete node;
            return true;
        }
        return false;
    }

private:
    // noncopyable
    NodeList(const NodeList&) = delete;
    NodeList& operator=(const NodeList&) = delete;

    void deleteHead()
    {
        Node* oldHead = mHead;
        mHead = mHead->next;
        delete oldHead;
    }

private:
    Node* mHead;
};

#endif