#include <xxhash.h>
#include <bitset>
#include <cstring>
#include "KVik/structures/IndexHashTable.h"

static constexpr size_t HT_VALUE_SIZE = KEY_SIZE + OFFSET_SIZE;
static constexpr size_t VALUES_IN_BUCKET = (8 * BUCKET_SIZE) / (1 + 8 * HT_VALUE_SIZE);
static constexpr size_t HEADER_SIZE = (VALUES_IN_BUCKET + 7) / 8;
static constexpr size_t HEADER_SIZE_BITS = HEADER_SIZE * 8;

bool compareKeys(const char *key1, const char *key2) {
    for (size_t i = 0; i < KEY_SIZE; i++) {
        if (key1[i] != key2[i]) {
            return false;
        }
    }
    return true;
}

void IndexHashTable::resize() {
    auto oldMemory = std::move(memory);
    auto oldBucketCnt = bucketCnt;
    size_t primesInd = std::lower_bound(PRIME_SIZES, PRIME_SIZES + PRIME_SIZES_COUNT, bucketCnt) - PRIME_SIZES;
    bucketCnt = PRIME_SIZES[++primesInd];
    // TODO : allocate place for bucketCnt too
    memory = allocator->alloc(bucketCnt * BUCKET_SIZE);
    // used instead of 'buffer' in order to keep bucket data same after call to 'put'
    char resizeBuffer[BUCKET_SIZE];
    for (uint64_t bucketIndex = 0; bucketIndex < oldBucketCnt; bucketIndex++) {
        oldMemory->read(bucketIndex * BUCKET_SIZE, BUCKET_SIZE, resizeBuffer);
        auto mask = *(std::bitset<VALUES_IN_BUCKET> *) resizeBuffer;
        for (size_t pos = 0; pos < VALUES_IN_BUCKET; pos++) {
            if (!mask[pos]) {
                continue;
            }
            Key key{};
            memcpy(&key, resizeBuffer + HEADER_SIZE + pos * HT_VALUE_SIZE, KEY_SIZE);
            int64_t value;
            memcpy(&value, resizeBuffer + HEADER_SIZE + pos * HT_VALUE_SIZE + KEY_SIZE, sizeof(value));
            put(key, value);
        }
    }
    allocator->dealloc(std::move(oldMemory));
}

IndexHashTable::IndexHashTable(std::unique_ptr<ZeroedMemoryAllocator> &&allocatorPtr) {
    bucketCnt = PRIME_SIZES[0];
    allocator = std::move(allocatorPtr);
    // TODO : allocate place for bucketsCount
    memory = allocator->alloc(bucketCnt * BUCKET_SIZE);
}

void IndexHashTable::put(const Key &key, int64_t value) {
    uint64_t hash = XXH3_64bits(&key, KEY_SIZE);
    uint64_t bucketIndex = hash % bucketCnt;
    memory->read(bucketIndex * BUCKET_SIZE, BUCKET_SIZE, buffer);
    auto mask = *(std::bitset<VALUES_IN_BUCKET> *) buffer;
    size_t pos = 0;
    for (; pos < VALUES_IN_BUCKET; ++pos) {
        if (mask[pos] && compareKeys(key.data, buffer + HEADER_SIZE + pos * HT_VALUE_SIZE)) {
            break;
        }
    }
    if (pos == VALUES_IN_BUCKET) {
        if (mask.all()) {
            resize();
            put(key, value);
            return;
        }
        pos = mask.flip()._Find_first();
        if (pos >= VALUES_IN_BUCKET) {
            resize();
            put(key, value);
            return;
        }
    }
    (*(std::bitset<VALUES_IN_BUCKET> *) buffer)[pos] = true;
    memcpy(buffer + HEADER_SIZE + pos * HT_VALUE_SIZE, &key, KEY_SIZE);
    memcpy(buffer + HEADER_SIZE + pos * HT_VALUE_SIZE + KEY_SIZE, &value, sizeof(value));
    memory->write(bucketIndex * BUCKET_SIZE, BUCKET_SIZE, buffer);
    ++_size;
}

int64_t IndexHashTable::get(Key const &key) {
    uint64_t hash = XXH3_64bits(&key, KEY_SIZE);
    uint64_t bucketIndex = hash % bucketCnt;
    memory->read(bucketIndex * BUCKET_SIZE, BUCKET_SIZE, buffer);
    auto mask = *(std::bitset<VALUES_IN_BUCKET> *) buffer;
    for (size_t pos = 0; pos < VALUES_IN_BUCKET; pos++) {
        if (!mask[pos]) {
            continue;
        }
        if (!compareKeys(key.data, buffer + HEADER_SIZE + pos * HT_VALUE_SIZE)) {
            continue;
        }
        return *(int64_t *) (buffer + HEADER_SIZE + pos * HT_VALUE_SIZE + KEY_SIZE);
    }
    //didn't find
    return 0;
}

void IndexHashTable::remove(Key const &key) {
    uint64_t hash = XXH3_64bits(&key, KEY_SIZE);
    uint64_t bucketNum = hash % bucketCnt;
    memory->read(bucketNum * BUCKET_SIZE, BUCKET_SIZE, buffer);
    auto mask = *(std::bitset<HEADER_SIZE_BITS> *) buffer;
    for (size_t pos = 0; pos < VALUES_IN_BUCKET; pos++) {
        if (!mask[pos]) {
            continue;
        }
        if (!compareKeys(key.data, buffer + HEADER_SIZE + pos * HT_VALUE_SIZE)) {
            continue;
        }
        (*(std::bitset<HEADER_SIZE_BITS> *) buffer)[pos] = false;
        memory->write(bucketNum * BUCKET_SIZE, HEADER_SIZE, buffer);
        _size--;
        return;
    }
}

uint64_t IndexHashTable::size() {
    return _size;
}
