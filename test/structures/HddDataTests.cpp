#include <gtest/gtest.h>
#include <memory>
#include <unordered_map>
#include <functional>

#include "KVik/structures/HddData.h"
#include "KVik/memory/DirectoryExtandableFileMemoryAllocator.h"

TEST(HddDataTest, EmptyWorks) {
    std::unique_ptr<MemoryAllocator> allocator = std::make_unique<DirectoryExtandableFileMemoryAllocator>(".", "test-hdd-data");
    HddData data(std::move(allocator));
    ASSERT_EQ(data.size(), 0);
}

static Value getRndValue() {
    Value value = {};
    for (char &i : value.data) {
        i = rand() % 100;
    }
    return value;
}

TEST(HddDataTest, AddAndRemoveOneRecordWorks) {
    std::unique_ptr<MemoryAllocator> allocator = std::make_unique<DirectoryExtandableFileMemoryAllocator>(".", "test-hdd-data");
    HddData data(std::move(allocator));
    ASSERT_EQ(data.size(), 0);
    Value value = {};
    for (int i = 0; i < VALUE_SIZE; ++i) {
        value.data[i] = (char)(i % 50) + 1;
    }
    Offset offset = data.write(value);
    ASSERT_EQ(data.size(), 1);
    Value readValue = data.read(offset);
    ASSERT_EQ(data.size(), 1);
    EXPECT_EQ(readValue, value);
    data.remove(offset);
    ASSERT_EQ(data.size(), 0);
}

TEST(HddDataTest, ManyRecordsStressTest) {
    std::unique_ptr<MemoryAllocator> allocator = std::make_unique<DirectoryExtandableFileMemoryAllocator>(".", "test-hdd-data");
    HddData data(std::move(allocator));
    std::unordered_map<Offset, Value> dataFileSimulator;
    constexpr int N = 1000;

    ASSERT_EQ(data.size(), 0);

    for (int i = 0; i < N; ++i) {
        Value value = getRndValue();
        dataFileSimulator[data.write(value)] = value;
        ASSERT_EQ(data.size(), i + 1);
    }

    for (auto const &[offset, value] : dataFileSimulator) {
        Value readValue = data.read(offset);
        EXPECT_EQ(readValue, value);
    }

    int cnt = 0;
    for (auto const &[offset, _] : dataFileSimulator) {
        data.remove(offset);
        ++cnt;
        EXPECT_EQ(data.size(), dataFileSimulator.size() - cnt);
    }

    ASSERT_EQ(data.size(), 0);
}

TEST(HddDataTest, WriteAfterRemoveWorkds) {
    std::unique_ptr<MemoryAllocator> allocator = std::make_unique<DirectoryExtandableFileMemoryAllocator>(".", "test-hdd-data");
    HddData data(std::move(allocator));
    ASSERT_EQ(data.size(), 0);

    Value value = getRndValue();
    Offset offset = data.write(value);
    EXPECT_EQ(data.size(), 1);
    Value readValue = data.read(offset);
    EXPECT_EQ(readValue, value);

    data.remove(offset);
    EXPECT_EQ(data.size(), 0);
    Offset newOffset = data.write(value);
    EXPECT_EQ(newOffset, offset);
    Value newValue = data.read(offset);
    EXPECT_EQ(newValue, value);
    EXPECT_EQ(data.size(), 1);
    data.remove(offset);

    ASSERT_EQ(data.size(), 0);
}