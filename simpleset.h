#ifndef SIMPLESET_H
#define SIMPLESET_H

#include <cstdint>
#include <vector>

struct SimpleSet {
  struct Bucket {
    int size;
    int count;
    uint64_t data[];
  };

  Bucket **buckets;
  int bucketCountBits;
  uint64_t bucketMask;
  int bucketCount;

  static constexpr auto initialBucketSize = 2;
  static constexpr auto bucketSizeExpansionFactor = 2;

  int simpleHash(uint64_t key) const {
    return ((uint32_t)key ^ (uint32_t)(key >> 32)) & bucketMask;
  }

  SimpleSet(int v) {
    int r = 0;
    while (v >>= 1) {
      r++;
    }

    bucketCountBits = r;
    bucketCount = 1 << bucketCountBits;
    bucketMask = bucketCount - 1;
    buckets = new Bucket *[bucketCount] { nullptr };
  }

  ~SimpleSet() {
    for(int i = 0; i < bucketCount; i++) {
      free(buckets[i]);
    }
    delete[] buckets;
  }

  void bucketAppend(int i, uint64_t key) {
    Bucket *cur = buckets[i];
    if(cur == nullptr) {
      Bucket *n = (Bucket *)malloc(sizeof(Bucket) + initialBucketSize * sizeof(uint64_t));
      n->size = initialBucketSize;
      n->count = 0;
      buckets[i] = n;
      cur = n;
    } else if(cur->count == cur->size) {
      int new_size = bucketSizeExpansionFactor * cur->size;
      Bucket *n = (Bucket *)realloc(cur, sizeof(Bucket) + new_size * sizeof(uint64_t));
      n->size = new_size;
      buckets[i] = n;
      cur = n;
    }
    cur->data[cur->count++] = key;
  }

  bool bucketContains(int i, uint64_t key) const {
    Bucket *cur = buckets[i];
    if(cur == nullptr) {
      return false;
    }
    for(int i = 0; i < cur->count; i++) {
      if(cur->data[i] == key)
        return true;
    }

    return false;
  }

  void insert(uint64_t key) {
    bucketAppend(simpleHash(key), key);
  }

  bool contains(uint64_t key) const {
    return bucketContains(simpleHash(key), key);
  }

  std::vector<uint64_t> toVector() {
    std::vector<uint64_t> result;

    for(int i = 0; i < bucketCount; i++) {
      Bucket *b;
      if((b = buckets[i]) != nullptr) {
        for(int j = 0; j < b->count; j++) {
          result.emplace_back(b->data[j]);
        }
      }
    }

    return result;
  }
};

#endif // SIMPLESET_H
