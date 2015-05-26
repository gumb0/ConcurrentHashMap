#include "ConcurrentHashMap.h"
#include "testHelpers.h"

#include <gtest/gtest.h>
#include <memory>

using namespace testing;

class HashmapTest : public Test
{
public:
    HashmapTest() : hashmap(Capacity) {}

protected:
    static const std::size_t Capacity;
    ConcurrentHashmap<int, int> hashmap;
};

const std::size_t HashmapTest::Capacity = 10;

TEST_F(HashmapTest, ConstructsWithGivenCapacity)
{
    ASSERT_EQ(Capacity, hashmap.capacity());
}

TEST_F(HashmapTest, InsertsSingleValue)
{
    hashmap.insert(1, 1);

    ASSERT_EQ(1, hashmap.size());
}

TEST_F(HashmapTest, FindsInsertedKey)
{
    int key = 1;
    int value = 2;
    hashmap.insert(key, value);

    ASSERT_TRUE(hashmap.find(key));
}

TEST_F(HashmapTest, DoesntFindNotInsertedKey)
{
    hashmap.insert(1, 2);

    ASSERT_FALSE(hashmap.find(2));
}

TEST_F(HashmapTest, GetsCopyOfInsertedValue)
{
    int key = 1;
    int value = 2;
    hashmap.insert(key, value);

    ASSERT_EQ(value, hashmap.getCopy(key));
}

TEST_F(HashmapTest, ThrowsWhenGettingCopyOfNotInsertedValue)
{
    hashmap.insert(1, 2);

    ASSERT_THROW(hashmap.getCopy(2), ConcurrentHashmapException);
}

TEST_F(HashmapTest, GetsInsertedValue)
{
    int key = 1;
    int value = 2;
    hashmap.insert(key, value);
    
    ConcurrentHashmap<int, int>::LockedValue lockedValue = hashmap.get(key);
    ASSERT_EQ(value, lockedValue.first);
}

TEST_F(HashmapTest, ThrowsWhenGettingNotInsertedValue)
{
    hashmap.insert(1, 2);

    ASSERT_THROW(hashmap.get(2), ConcurrentHashmapException);
}

TEST_F(HashmapTest, DeletesValue)
{
    int key = 1;
    hashmap.insert(key, 2);

    hashmap.erase(key);

    ASSERT_EQ(0, hashmap.size());
    ASSERT_FALSE(hashmap.find(key));
}

TEST_F(HashmapTest, EraseDoesNothingIfKeyNotFound)
{
    hashmap.insert(1, 2);

    hashmap.erase(3);

    ASSERT_EQ(1, hashmap.size());
}

TEST_F(HashmapTest, InsertOverwritesValueIfKeyAlreadyExists)
{
    hashmap.insert(1, 1);
    hashmap.insert(1, 10);

    ASSERT_EQ(10, hashmap.getCopy(1));
}

class HashmapEqualHashTest : public Test
{
public:
    HashmapEqualHashTest() : hashmap(Capacity, 16, dummyIntHash) {}

protected:
    static const std::size_t Capacity;
    ConcurrentHashmap<int, int, IntHashFunction> hashmap;
};

const std::size_t HashmapEqualHashTest::Capacity = 10;

TEST_F(HashmapEqualHashTest, GetsValueCopies)
{
    int key1 = 1;
    int value1 = 2;
    hashmap.insert(key1, value1);
    int key2 = 3;
    int value2 = 4;
    hashmap.insert(key2, value2);

    ASSERT_EQ(value1, hashmap.getCopy(key1));
    ASSERT_EQ(value2, hashmap.getCopy(key2));
}

TEST_F(HashmapEqualHashTest, GetsValues)
{
    int key1 = 1;
    int value1 = 2;
    hashmap.insert(key1, value1);
    int key2 = 3;
    int value2 = 4;
    hashmap.insert(key2, value2);

    ConcurrentHashmap<int, int>::LockedValue lockedValue1 = hashmap.get(key1);
    ASSERT_EQ(value1, lockedValue1.first);
    lockedValue1.second.unlock();

    ConcurrentHashmap<int, int>::LockedValue lockedValue2 = hashmap.get(key2);
    ASSERT_EQ(value2, lockedValue2.first);
}

TEST_F(HashmapEqualHashTest, DeletesOneOfValues)
{
    int key1 = 1;
    hashmap.insert(key1, 2);
    int key2 = 3;
    hashmap.insert(key2, 4);

    hashmap.erase(key1);

    ASSERT_FALSE(hashmap.find(key1));
    ASSERT_TRUE(hashmap.find(key2));
}

TEST_F(HashmapEqualHashTest, EraseDoesNothingIfKeyNotFound)
{
    hashmap.insert(1, 2);

    hashmap.erase(3);

    ASSERT_EQ(1, hashmap.size());
}

TEST(InvalidHashmapTest, ThrowsIfInvalidCapacity)
{
    std::unique_ptr<ConcurrentHashmap<int, int>> p;

    ASSERT_THROW(p.reset(new ConcurrentHashmap<int, int>(0)), ConcurrentHashmapException);
}

TEST(InvalidHashmapTest, ThrowsIfInvalidConcurrencyLevel)
{
    std::unique_ptr<ConcurrentHashmap<int, int>> p;

    ASSERT_THROW(p.reset(new ConcurrentHashmap<int, int>(1, 0)), ConcurrentHashmapException);
}

TEST(HashmapStringKeysTest, WorksWithStringKeys)
{
    ConcurrentHashmap<std::string, std::string> hashmap(100);
    std::string key1 = "abc";
    std::string value1 = "bbb";
    hashmap.insert(key1, value1);
    std::string key2 = "def";
    std::string value2 = "aaa";
    hashmap.insert(key2, value2);

    ASSERT_EQ(2, hashmap.size());
    ASSERT_EQ(value1, hashmap.getCopy(key1));
    ASSERT_EQ(value2, hashmap.getCopy(key2));

    ConcurrentHashmap<std::string, std::string>::LockedValue lockedValue1 = hashmap.get(key1);
    ASSERT_EQ(value1, lockedValue1.first);
    lockedValue1.second.unlock();

    ConcurrentHashmap<std::string, std::string>::LockedValue lockedValue2 = hashmap.get(key2);
    ASSERT_EQ(value2, lockedValue2.first);
    lockedValue2.second.unlock();

    hashmap.erase(key1);
    hashmap.erase(key2);

    ASSERT_EQ(0, hashmap.size());
    ASSERT_FALSE(hashmap.find(key1));
    ASSERT_FALSE(hashmap.find(key2));
}

namespace
{
    struct Value
    {
        static int copied;
        Value() {}
        Value(const Value&) { ++copied; }
        Value& operator = (const Value&) { ++copied; return *this; }
    };

    int Value::copied = 0;
}

TEST(HashmapGetTest, GetDoesntMakeValueCopies)
{
    Value::copied = 0;
    ConcurrentHashmap<int, Value> hashmap(10);
    int key = 1;
    hashmap.insert(key, Value()); // 1 copy is made here

    ConcurrentHashmap<int, Value>::LockedValue lv = hashmap.get(key);

    ASSERT_EQ(1, Value::copied);
}
