#include "HashMap.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

using namespace dnsge;

struct IntHasher {
    size_t operator()(int y) const {
        auto x = static_cast<unsigned int>(y);
        x = ((x >> 16) ^ x) * 0x45d9f3b;
        x = ((x >> 16) ^ x) * 0x45d9f3b;
        x = (x >> 16) ^ x;
        return x;
    }
};

TEST(HashMap, InsertNew) {
    HashMap<int, std::string, IntHasher> map;
    map.insert({5, "Hello, world!"});
    map.insert({2, "wow!"});

    ASSERT_NE(map.find(5), map.end());
    ASSERT_NE(map.find(2), map.end());
    ASSERT_EQ(map.find(3), map.end());

    ASSERT_EQ(map.find(5)->first, 5);
    ASSERT_EQ(map.find(5)->second, "Hello, world!");
    ASSERT_EQ(map.find(2)->first, 2);
    ASSERT_EQ(map.find(2)->second, "wow!");
}

TEST(HashMap, InsertExisting) {
    HashMap<int, std::string, IntHasher> map;
    map.insert({5, "123"});
    map.insert({5, "456"});

    ASSERT_NE(map.find(5), map.end());
    ASSERT_EQ(map.find(5)->first, 5);
    ASSERT_EQ(map.find(5)->second, "123");
}

TEST(HashMap, InsertAfterErase) {
    HashMap<int, std::string, IntHasher> map;
    map.insert({5, "123"});
    map.erase(5);
    map.insert({5, "456"});

    ASSERT_NE(map.find(5), map.end());
    ASSERT_EQ(map.find(5)->first, 5);
    ASSERT_EQ(map.find(5)->second, "456");
}

TEST(HashMap, AutoGrow) {
    HashMap<int, std::string, IntHasher> map(4);
    ASSERT_EQ(map.capacity(), 4UL);

    map.insert({1, "123"});
    map.insert({2, "123"});
    map.insert({3, "123"});
    map.insert({4, "123"});
    map.insert({5, "123"});

    ASSERT_GT(map.capacity(), 4UL);

    size_t cap = map.capacity();
    for (size_t i = 1; i < cap; ++i) {
        int val = i * 10;
        map.insert({val, "123"});
    }
    ASSERT_GE(map.capacity(), cap);
}

TEST(HashMap, EraseExisting) {
    HashMap<int, std::string, IntHasher> map;
    ASSERT_EQ(map.size(), 0UL);

    map.insert({1, "abc"});
    map.insert({2, "def"});
    ASSERT_EQ(map.size(), 2UL);

    bool deleted = map.erase(1);
    ASSERT_TRUE(deleted);
    ASSERT_EQ(map.size(), 1UL);

    deleted = map.erase(2);
    ASSERT_TRUE(deleted);
    ASSERT_EQ(map.size(), 0UL);
}

TEST(HashMap, EraseNonExisting) {
    HashMap<int, std::string, IntHasher> map;
    ASSERT_EQ(map.size(), 0UL);

    map.insert({1, "abc"});
    map.insert({2, "def"});
    ASSERT_EQ(map.size(), 2UL);

    bool deleted = map.erase(1);
    ASSERT_TRUE(deleted);
    ASSERT_EQ(map.size(), 1UL);

    deleted = map.erase(5);
    ASSERT_FALSE(deleted);
    ASSERT_EQ(map.size(), 1UL);
}

TEST(HashMap, EraseIterator) {
    HashMap<int, std::string, IntHasher> map;

    map.insert({1, "abc"});
    map.insert({2, "def"});

    auto it = map.find(1);
    ASSERT_NE(it, map.end());
    bool deleted = map.erase(it);
    ASSERT_TRUE(deleted);

    it = map.find(5);
    ASSERT_EQ(it, map.end());
    deleted = map.erase(it);
    ASSERT_FALSE(deleted);
}

TEST(HashMap, Copy) {
    HashMap<int, std::string, IntHasher> map1;
    map1.insert({1, "abc"});
    map1.insert({2, "def"});

    HashMap<int, std::string, IntHasher> map2(map1);

    // Size should be the same after copy
    ASSERT_EQ(map1.size(), 2UL);
    ASSERT_EQ(map2.size(), 2UL);

    // Elements present in original should now be found in both
    ASSERT_NE(map1.find(1), map1.end());
    ASSERT_NE(map2.find(1), map2.end());
    ASSERT_NE(map1.find(2), map1.end());
    ASSERT_NE(map2.find(2), map2.end());

    // Elements in neither should still not be there
    ASSERT_EQ(map1.find(3), map1.end());
    ASSERT_EQ(map2.find(3), map2.end());

    // Erasing in one map should not affect the other
    map1.erase(1);
    map2.erase(2);
    ASSERT_EQ(map1.find(2)->second, "def");
    ASSERT_EQ(map2.find(1)->second, "abc");
}

TEST(HashMap, CopyAssign) {
    HashMap<int, std::string, IntHasher> map1;
    map1.insert({1, "abc"});
    map1.insert({2, "def"});

    HashMap<int, std::string, IntHasher> map2;
    map2.insert({1, "hello"});
    map2.insert({3, "wow"});
    map2.insert({5, "cool"});

    // Perform copy
    map2 = map1;

    // Size should be the same after copy
    ASSERT_EQ(map1.size(), 2UL);
    ASSERT_EQ(map2.size(), 2UL);

    // Element 5 should not be in map2
    ASSERT_FALSE(map2.erase(5));

    // Elements present in original should now be found in both
    ASSERT_NE(map1.find(1), map1.end());
    ASSERT_NE(map2.find(1), map2.end());
    ASSERT_NE(map1.find(2), map1.end());
    ASSERT_NE(map2.find(2), map2.end());

    // Elements in neither should still not be there
    ASSERT_EQ(map1.find(3), map1.end());
    ASSERT_EQ(map2.find(3), map2.end());

    // Erasing in one map should not affect the other
    map1.erase(1);
    map2.erase(2);
    ASSERT_EQ(map1.find(2)->second, "def");
    ASSERT_EQ(map2.find(1)->second, "abc");
}

TEST(HashMap, Move) {
    HashMap<int, std::string, IntHasher> map1;
    map1.insert({1, "abc"});
    map1.insert({2, "def"});

    ASSERT_FALSE(map1.empty());

    // Perform move
    HashMap<int, std::string, IntHasher> map2(std::move(map1));

    // map2 should have old size as map1
    ASSERT_TRUE(map1.empty());
    ASSERT_EQ(map1.size(), 0UL);
    ASSERT_FALSE(map2.empty());
    ASSERT_EQ(map2.size(), 2UL);

    // Elements present in original should now be found in only new
    ASSERT_EQ(map1.find(1), map1.end());
    ASSERT_NE(map2.find(1), map2.end());
    ASSERT_EQ(map1.find(2), map1.end());
    ASSERT_NE(map2.find(2), map2.end());

    // Elements in neither should still not be there
    ASSERT_EQ(map1.find(3), map1.end());
    ASSERT_EQ(map2.find(3), map2.end());

    // Erasing in one map should not affect the other
    ASSERT_FALSE(map1.erase(1));
    ASSERT_FALSE(map1.erase(2));

    // Elements should be in map2
    ASSERT_EQ(map2.find(1)->second, "abc");
    ASSERT_EQ(map2.find(2)->second, "def");
    ASSERT_TRUE(map2.erase(1));
    ASSERT_TRUE(map2.erase(2));
}

TEST(HashMap, MoveAssign) {
    HashMap<int, std::string, IntHasher> map1;
    map1.insert({1, "abc"});
    map1.insert({2, "def"});

    HashMap<int, std::string, IntHasher> map2;
    map2.insert({1, "hello"});
    map2.insert({3, "wow"});
    map2.insert({5, "cool"});

    // Perform move
    map2 = std::move(map1);

    // map2 should have old size as map1
    ASSERT_TRUE(map1.empty());
    ASSERT_EQ(map1.size(), 0UL);
    ASSERT_FALSE(map2.empty());
    ASSERT_EQ(map2.size(), 2UL);

    // Element 5 should not be in map2
    ASSERT_EQ(map2.find(5), map2.end());
    ASSERT_FALSE(map2.erase(5));

    // Elements present in original should now be found in only new
    ASSERT_EQ(map1.find(1), map1.end());
    ASSERT_NE(map2.find(1), map2.end());
    ASSERT_EQ(map1.find(2), map1.end());
    ASSERT_NE(map2.find(2), map2.end());

    // Elements in neither should still not be there
    ASSERT_EQ(map1.find(3), map1.end());
    ASSERT_EQ(map2.find(3), map2.end());

    // Erasing in one map should not affect the other
    ASSERT_FALSE(map1.erase(1));
    ASSERT_FALSE(map1.erase(2));

    // Elements should be in map2
    ASSERT_EQ(map2.find(1)->second, "abc");
    ASSERT_EQ(map2.find(2)->second, "def");
    ASSERT_TRUE(map2.erase(1));
    ASSERT_TRUE(map2.erase(2));
}

TEST(HashMap, MovedMapIsValid) {
    HashMap<int, std::string, IntHasher> map1;
    map1.insert({1, "abc"});
    map1.insert({2, "def"});

    HashMap<int, std::string, IntHasher> map2(std::move(map1));

    ASSERT_TRUE(map1.empty());
    ASSERT_EQ(map1.size(), 0UL);
    ASSERT_EQ(map1.capacity(), 0UL);

    map1.insert({1, "hello"});
    map1.insert({3, "world"});
    ASSERT_FALSE(map1.empty());
    ASSERT_NE(map1.capacity(), 0UL);
    ASSERT_EQ(map1.size(), 2UL);

    size_t cap = map1.capacity();
    for (size_t i = 1; i < cap; ++i) {
        int val = i * 10;
        map1.insert({val, "123"});
    }

    ASSERT_GE(map1.capacity(), cap);
}

TEST(HashMap, ClearedIsValid) {
    HashMap<int, std::string, IntHasher> map;
    map.insert({1, "abc"});
    map.insert({2, "def"});

    map.clear();

    ASSERT_NE(map.capacity(), 0UL);
    ASSERT_EQ(map.size(), 0UL);
    ASSERT_EQ(map.find(1), map.end());
    ASSERT_EQ(map.find(2), map.end());
    ASSERT_EQ(map.find(3), map.end());

    map.insert({1, "123"});
    map.insert({2, "456"});

    ASSERT_EQ(map.find(1)->second, "123");
    ASSERT_EQ(map.find(2)->second, "456");
}

TEST(HashMap, OperatorSquareBracket) {
    HashMap<int, std::vector<int>, IntHasher> map;

    map[1].push_back(5);
    map[1].push_back(10);
    map[1].push_back(15);

    auto it = map.find(1);
    ASSERT_NE(it, map.end());
    ASSERT_EQ(it->second.size(), 3UL);
}

TEST(HashMap, MoveCount) {
    static size_t moveCount = 0;
    class Mover {
    public:
        Mover(size_t data)
            : data_(data){};

        Mover(Mover &&other) noexcept
            : data_(other.data_) {
            other.data_ = 0;
            ++moveCount;
        }

    private:
        size_t data_;
    };

    HashMap<int, Mover, IntHasher> map;

    // Temporary should be moved
    map.insert({1, Mover{5}});
    ASSERT_EQ(moveCount, 2UL);

    moveCount = 0;
    HashMap<int, Mover, IntHasher> map2(std::move(map));
    ASSERT_EQ(moveCount, 0UL); // no move with internals shifting about

    moveCount = 0;
    map2.reserve(map2.capacity() * 2); // trigger rehash
    ASSERT_EQ(moveCount, 1UL);         // should have moved the one element
}

TEST(HashMap, InsertionSemantics) {
    HashMap<int, std::string, IntHasher> map;
    std::pair<int, std::string> keyValue1{5, "Hello"};
    std::pair<int, std::string> keyValue2{10, "World"};

    map.insert(keyValue1);
    map.insert(std::move(keyValue2));

    ASSERT_EQ(keyValue1.second, "Hello");
    ASSERT_EQ(keyValue2.second, "");
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
