#pragma once
#include "core/core.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>

namespace core {

template <typename T, size_t L> class array {

public:
  T cdata[L];
  constexpr T &operator[](size_t index) { return cdata[index]; }

  constexpr Result<T *> at(size_t index) {
    if (index >= L) [[unlikely]] {
      return Result<T *>{.error = "out of bounds", .ok = false};
    }
    return Result<T *>{.value = &cdata[index], .ok = true};
  }

  constexpr size_t size() const { return L; }

  constexpr void memset(T value) {
    for (uint32_t i = 0; i < L; i++) {
      cdata[i] = value;
    }
  }

  constexpr T *data() { return cdata; };
};

template <typename T, typename... Args>
array(T, Args...) -> array<T, 1 + sizeof...(Args)>;

template <typename T> class vector {
  T *cdata;
  size_t clength;
  size_t ccapacity;
  void reallocate() {
    ccapacity = ccapacity == 0 ? 1 : ccapacity * 2;
    T *temp = (T *)realloc(cdata, ccapacity * sizeof(T));
    if (!temp) {
      abort();
    }
    cdata = temp;
  }

public:
  vector() : cdata(nullptr), clength(0), ccapacity(0) {}

  vector(std::initializer_list<T> list) {
    clength = list.size();
    ccapacity = clength * 2;
    cdata = (T *)malloc(ccapacity * sizeof(T));
    if (!cdata)
      abort();
    memcpy(cdata, list.begin(), clength * sizeof(T));
  }

  vector(size_t count) {
    clength = count;
    ccapacity = clength * 2;
    cdata = (T *)malloc(ccapacity * sizeof(T));
    if (!cdata) {
      abort();
    }
  }

  vector(size_t count, const T &value) {
    clength = count;
    ccapacity = clength * 2;
    cdata = (T *)malloc(ccapacity * sizeof(T));
    if (!cdata) {
      abort();
    }
    fill(value);
  }

  vector(const vector<T> &vec) {
    clength = vec.clength;
    ccapacity = vec.ccapacity;
    cdata = (T *)malloc(ccapacity * sizeof(T));
    if (!cdata) {
      abort();
    }
    memcpy(cdata, vec.cdata, ccapacity * sizeof(T));
  }

  vector(vector<T> &&vec) {
    clength = vec.clength;
    ccapacity = vec.ccapacity;
    cdata = vec.cdata;
    vec.cdata = nullptr;
    vec.clength = 0;
    vec.ccapacity = 0;
  }

  T &operator[](size_t index) { return cdata[index]; }
  const T &operator[](size_t index) const { return cdata[index]; }

  Result<T *> at(size_t index) {
    if (index >= clength) [[unlikely]] {
      return Result<T *>{.error = "out of bounds", .ok = false};
    }
    return Result<T *>{.value = &cdata[index], .ok = true};
  }

  Result<const T *> at(size_t index) const {
    if (index >= clength) [[unlikely]] {
      return Result<const T *>{.error = "out of bounds", .ok = false};
    }
    return Result<const T *>{.value = &cdata[index], .ok = true};
  }

  void append(const T &value) {
    if (clength == ccapacity) {
      reallocate();
    }
    cdata[clength++] = value;
  }

  void pop() {
    if (clength == 0) {
      return;
    }
    clength--;
  }

  void fill(const T &value) {
    for (size_t i = 0; i < clength; i++) {
      cdata[i] = value;
    }
  }

  int reserve(size_t count) {
    if (ccapacity >= count) {
      return 0;
    }
    ccapacity = count;
    T *temp = (T *)realloc(cdata, ccapacity * sizeof(T));
    if (!temp) {
      abort();
    }
    cdata = temp;
    return 0;
  }

  T *data() { return cdata; }

  size_t capacity() const { return ccapacity; }
  size_t length() const { return clength; }

  ~vector() { free(cdata); }
};

template <typename T, size_t L> class stack {
  array<T, L> data;
  uint32_t ptr;

public:
  constexpr inline int insert(const T &value) {
    if (ptr >= L) [[unlikely]] {
      return -1;
    }
    data[ptr++] = value;
    return 0;
  }
  constexpr inline Result<T *> top() {
    if (ptr == 0)
      return Result<T *>{.error = "out of stack bounds", .ok = false};

    return Result<T *>{.value = &data[ptr - 1], .ok = true};
  }
  constexpr int pop() {
    if (ptr == 0) [[unlikely]] {
      return -1;
    }
    ptr--;
    return 0;
  }

  constexpr inline uint32_t size() const { return ptr; }
  constexpr inline void clear() { ptr = 0; }
};

template <size_t L>
concept PowerOfTwo = L > 0 && (L & (L - 1)) == 0;

template <typename T, size_t L>
  requires PowerOfTwo<L>
class queue {
  T data[L];
  uint32_t head = 0;
  uint32_t tail = 0;
  uint32_t count = 0;

public:
  constexpr inline int insert(const T &value) {
    if (count == L) [[unlikely]] {
      return -1;
    }
    data[tail] = value;
    // Optimization: Use bitwise AND instead of modulo (%)
    tail = (tail + 1) & (L - 1);
    count++;
    return 0;
  }

  constexpr inline T &front() { return data[head]; }

  constexpr inline int pop() {
    if (count == 0) [[unlikely]] {
      return -1;
    }
    head = (head + 1) & (L - 1);
    count--;
    return 0;
  }

  constexpr inline uint32_t size() const { return count; }

  constexpr inline void clear() {
    head = 0;
    tail = 0;
    count = 0;
  }
};

template <typename T, size_t L>
  requires(L % 32 == 0)
class slotMap {
  array<T, L> data;
  uint32_t slotBitMap[L / 32]{};

  int findFreeSlot() {
    for (uint32_t i = 0; i < L / 32; i++) {
      if (~slotBitMap[i] == 0)
        continue;
      return __builtin_ctz(~slotBitMap[i]) + i * 32;
    }
    return -1;
  }

  int setSlotState(uint32_t slot, bool state) {
    if (slot >= L) [[unlikely]] {
      return -1;
    }
    if (state) {
      slotBitMap[slot / 32] |= (UINT32_C(1) << slot % 32);
    } else {
      slotBitMap[slot / 32] &= ~(UINT32_C(1) << slot % 32);
    }
    return 0;
  }

public:
  T &operator[](size_t index) { return data[index]; }

  Result<T *> at(size_t index) {
    if (index >= L) [[unlikely]] {
      return Result<T *>{.error = "out of bounds", .ok = false};
    }
    return Result<T *>{.value = &data[index], .ok = true};
  }

  int insert(const T &value) {
    int index = findFreeSlot();
    if (index == -1) {
      return -1;
    }
    data[index] = value;
    setSlotState(index, true);
    return index;
  }

  bool isOccupied(uint32_t index) const {
    if (index >= L) {
      return false;
    }
    return slotBitMap[index / 32] & (UINT32_C(1) << index % 32);
  }

  int remove(uint32_t index) {
    if (!isOccupied(index)) {
      return -1;
    }
    setSlotState(index, false);
    return 0;
  }
};

template <typename T> class slotMap<T, 64> {
  array<T, 64> data{};
  uint64_t slotMap{};

public:
  T &operator[](size_t index) { return data[index]; }

  Result<T *> at(size_t index) {
    if (index >= 64) [[unlikely]] {
      return Result<T *>{.error = "out of bounds", .ok = false};
    }
    return Result<T *>{.value = &data[index], .ok = true};
  }

  int insert(const T &value) {
    if (~slotMap == 0) [[unlikely]]
      return -1;
    int index = __builtin_ctzll(~slotMap);
    data[index] = value;
    slotMap |= (UINT64_C(1) << index);
    return index;
  }

  bool isOccupied(uint32_t index) const {
    if (index >= 64)
      return false;
    return slotMap & (1ULL << index);
  }

  int remove(uint32_t index) {
    if (!isOccupied(index)) [[unlikely]] {
      return -1;
    }
    slotMap &= ~(1ULL << index);
    return 0;
  }
};

} // namespace core
