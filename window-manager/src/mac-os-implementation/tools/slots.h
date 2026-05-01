#pragma once
#include "../../Result.hpp"
#include <cstdint>
#include <cstdlib>

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

  constexpr void fill(const T &value) {
    for (uint32_t i = 0; i < L; i++) {
      cdata[i] = value;
    }
  }

  constexpr T *data() { return cdata; };
};

template <typename T> class slotMap {
  array<T, 64> data;
  uint64_t slotBitMap{};

public:
  Result<T *> operator[](size_t index) {
    if (!isOccupied(index)) {
      return Result<T *>{
          .error = "Slot not occupied.",
          .ok = false,
      };
    }
    return Result<T *>{.value = &data[index], .ok = true};
  }

  int insert(const T &value) {
    if (~slotBitMap == 0) [[unlikely]]
      return -1;
    int index = __builtin_ctzll(~slotBitMap);
    data[index] = value;
    slotBitMap |= (UINT64_C(1) << index);
    return index;
  }

  inline bool isOccupied(uint32_t index) const {
    if (index >= 64)
      return false;
    return slotBitMap & (1ULL << index);
  }

  int remove(uint32_t index) {
    if (!isOccupied(index)) [[unlikely]] {
      return -1;
    }
    slotBitMap &= ~(1ULL << index);
    return 0;
  }
};
