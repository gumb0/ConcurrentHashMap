#include "ConcurrentHashMap.h"
#include "testHelpers.h"

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <vector>

using namespace testing;

namespace
{
    typedef std::function<void(int)> HashmapFunction;

    template<class Hashmap>
    HashmapFunction createInserter(Hashmap& hashmap, int count)
    {
        return [&hashmap, count](int threadIndex)
        {
            for (int i = 0; i < count; ++i)
                hashmap.insert(threadIndex * count + i, i * i);
        };
    }

    template<class Hashmap>
    HashmapFunction createFinder(Hashmap& hashmap, int count)
    {
        return [&hashmap, count](int threadIndex)
        {
            for (int i = 0; i < count; ++i)
                hashmap.find(threadIndex * count + i);
        };
    }

    template<class Hashmap>
    HashmapFunction createEraser(Hashmap& hashmap, int count)
    {
        return [&hashmap, count](int threadIndex)
        {
            for (int i = 0; i < count; ++i)
                hashmap.erase(threadIndex * count + i);
        };
    }

    template<class Hashmap>
    HashmapFunction createGetter(Hashmap& hashmap, int count)
    {
        return [&hashmap, count](int threadIndex)
        {
            for (int i = 0; i < count; ++i)
            {
                try
                {
                    typename Hashmap::LockedValue valueAndLock = hashmap.get(threadIndex * count + i);
                    int value = valueAndLock.first;
                }
                catch (...) {}
            }
        };
    }
}

class ConcurrentHashmapTest : public Test
{
public:
    ConcurrentHashmapTest() : hashmap(Capacity) {}

protected:
    static const int Capacity;
    static const int ThreadNumber;
    static const int ValuesPerThread;
    static const int TotalValues;
    ConcurrentHashmap<int, int> hashmap;
    std::vector<std::thread> threads;
};

const int ConcurrentHashmapTest::Capacity = 50000;
const int ConcurrentHashmapTest::ThreadNumber = 100;
const int ConcurrentHashmapTest::ValuesPerThread = 1000;
const int ConcurrentHashmapTest::TotalValues = ThreadNumber * ValuesPerThread;

TEST_F(ConcurrentHashmapTest, InsertsConcurrently)
{
    for (int i = 0; i < ThreadNumber; ++i)
        threads.push_back(std::thread(createInserter(hashmap, ValuesPerThread), i));

    for (std::thread& t : threads)
        t.join();

    ASSERT_EQ(TotalValues, hashmap.size());
    for (int i = 0; i < TotalValues; ++i)
        ASSERT_TRUE(hashmap.find(i));
}

TEST_F(ConcurrentHashmapTest, InsertsAndReadsConcurrently)
{
    for (int i = 0; i < ThreadNumber; ++i)
    {
        if (i % 2)
            threads.push_back(std::thread(createFinder(hashmap, ValuesPerThread), i));
        else
            threads.push_back(std::thread(createInserter(hashmap, ValuesPerThread), i));
    }

    for (std::thread& t : threads)
        t.join();

    // just checking that it doesn't crash
}

TEST_F(ConcurrentHashmapTest, DeletesConcurrently)
{
    for (int i = 0; i < TotalValues; ++i)
        hashmap.insert(i, rand());
    
    for (int i = 0; i < ThreadNumber; ++i)
        threads.push_back(std::thread(createEraser(hashmap, ValuesPerThread), i));

    for (std::thread& t : threads)
        t.join();

    ASSERT_EQ(0, hashmap.size());
    for (int i = 0; i < TotalValues; ++i)
        ASSERT_FALSE(hashmap.find(i));
}

TEST_F(ConcurrentHashmapTest, DeletesAndReadsConcurrently)
{
    for (int i = 0; i < TotalValues; ++i)
        hashmap.insert(i, rand());

    for (int i = 0; i < ThreadNumber; ++i)
    {
        threads.push_back(std::thread(createFinder(hashmap, ValuesPerThread), i));
        threads.push_back(std::thread(createEraser(hashmap, ValuesPerThread), i));
    }

    for (std::thread& t : threads)
        t.join();

    ASSERT_EQ(0, hashmap.size());
    for (int i = 0; i < TotalValues; ++i)
    {
        ASSERT_FALSE(hashmap.find(i));
    }
}

TEST_F(ConcurrentHashmapTest, InsertsAndReadsAndDeletesConcurrently)
{
    int threadNumber = 300;
    int valuesPerThread = 3000;
    for (int i = 0; i < threadNumber; ++i)
    {
        threads.push_back(std::thread(createInserter(hashmap, valuesPerThread), rand() % threadNumber));
        threads.push_back(std::thread(createFinder(hashmap, valuesPerThread), rand() % threadNumber));
        threads.push_back(std::thread(createEraser(hashmap, valuesPerThread), rand() % threadNumber));
        threads.push_back(std::thread(createGetter(hashmap, valuesPerThread), rand() % threadNumber));
    }

    for (std::thread& t : threads)
        t.join();

    // just checking that it doesn't crash
}


TEST_F(ConcurrentHashmapTest, DeletesAndGetsConcurrently)
{
    for (std::size_t i = 0; i < 100; ++i)
    {
        hashmap.insert(i, rand());

        std::thread getThread([&]
        {
            try
            {
                ConcurrentHashmap<int, int>::LockedValue valueAndLock = hashmap.get(i);
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                ASSERT_EQ(1, hashmap.size());
            }
            catch (...) {}
        });

        std::thread eraseThread([&]{
            hashmap.erase(i);
        });

        getThread.join();
        eraseThread.join();
    }
}

class ConcurrentHashmapEqualHashTest : public Test
{
public:
    ConcurrentHashmapEqualHashTest() : hashmap(Capacity, 16, dummyIntHash) {}

protected:
    static const int Capacity;
    static const int ThreadNumber;
    static const int ValuesPerThread;
    static const int TotalValues;
    ConcurrentHashmap<int, int, IntHashFunction> hashmap;
    std::vector<std::thread> threads;
};

const int ConcurrentHashmapEqualHashTest::Capacity = 100;
const int ConcurrentHashmapEqualHashTest::ThreadNumber = 100;
const int ConcurrentHashmapEqualHashTest::ValuesPerThread = 1000;
const int ConcurrentHashmapEqualHashTest::TotalValues = ThreadNumber * ValuesPerThread;

TEST_F(ConcurrentHashmapEqualHashTest, InsertsConcurrentlyValuesWithEqualHash)
{
    for (int i = 0; i < ThreadNumber; ++i)
        threads.push_back(std::thread(createInserter(hashmap, ValuesPerThread), i));

    for (std::thread& t : threads)
        t.join();

    ASSERT_EQ(TotalValues, hashmap.size());
    for (int i = 0; i < TotalValues; ++i)
        ASSERT_TRUE(hashmap.find(i));
}

TEST_F(ConcurrentHashmapEqualHashTest, DeletesAndReadsConcurrentlyValuesWithEqualHash)
{
    for (int i = 0; i < TotalValues; ++i)
        hashmap.insert(i, rand());

    for (int i = 0; i < ThreadNumber; ++i)
    {
        threads.push_back(std::thread(createFinder(hashmap, ValuesPerThread), i));
        threads.push_back(std::thread(createEraser(hashmap, ValuesPerThread), i));
    }

    for (std::thread& t : threads)
        t.join();

    ASSERT_EQ(0, hashmap.size());
    for (int i = 0; i < TotalValues; ++i)
    {
        ASSERT_FALSE(hashmap.find(i));
    }
}
