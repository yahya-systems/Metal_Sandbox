#pragma once

#include "core/core.hpp"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace core {
// class LinearAllocator {
//   uint8_t *buffer;
//   size_t offset;
//   size_t capacity;
//
// public:
//   LinearAllocator(size_t size) : offset(0), capacity(size) {
//     buffer = (uint8_t *)malloc(size);
//   }
//
//   void *alloc(size_t size, size_t align = 8) {
//     size_t aligned = (offset + align - 1) & ~(align - 1);
//     if (aligned + size > capacity)
//       abort();
//     offset = aligned + size;
//     return buffer + aligned;
//   }
//   void reset() { offset = 0; }
//
//   ~LinearAllocator() { free(buffer); }
// };

class LinearAllocator {
  unsigned char *buffer = nullptr;
  unsigned char *offset;
  size_t capacity;

public:
  LinearAllocator() = delete;

  LinearAllocator(size_t size) : capacity(size) {
    buffer = (unsigned char *)malloc(size);
    if (!buffer) {
      offset = nullptr;
      return;
    }
    offset = buffer;
  }

  LinearAllocator(const LinearAllocator &allocator) = delete;

  LinearAllocator(LinearAllocator &&allocator) {
    buffer = allocator.buffer;
    offset = allocator.offset;
    capacity = allocator.capacity;

    allocator.buffer = nullptr;
    allocator.offset = nullptr;
    allocator.capacity = 0;
  }

  void *alloc(size_t size, size_t alignment = 8) {
    assert(is_power_of_two(alignment));
    uintptr_t addr = (uintptr_t)offset;
    size_t padding = (alignment - (addr & (alignment - 1))) & (alignment - 1);

    if ((offset - buffer) + padding + size > capacity) {
      return nullptr;
    }

    offset += padding;
    void *result = offset;
    offset += size;

    return result;
  }

  void reset() { offset = buffer; }

  int expand(size_t size) {
    if (size <= capacity) {
      return 0;
    }

    capacity = size;
    size_t _offset = offset - buffer;

    void *temp = realloc(buffer, capacity);
    if (!temp) {
      return -1;
    }
    buffer = (unsigned char *)temp;
    offset = buffer + _offset;
    return 0;
  }

  ~LinearAllocator() { free(buffer); }
};

class StackAllocator {
  unsigned char *buffer = nullptr;
  unsigned char *offset;
  size_t capacity;

public:
  StackAllocator() = delete;

  StackAllocator(size_t size) : capacity(size) {
    buffer = (unsigned char *)malloc(size);
    if (!buffer) {
      offset = nullptr;
      return;
    }
    offset = buffer;
  }

  StackAllocator(const LinearAllocator &allocator) = delete;

  StackAllocator(StackAllocator &&allocator) {
    buffer = allocator.buffer;
    offset = allocator.offset;
    capacity = allocator.capacity;

    allocator.buffer = nullptr;
    allocator.offset = nullptr;
    allocator.capacity = 0;
  }

  void *alloc(size_t size, size_t alignment = 8) {
    assert(is_power_of_two(alignment));
    uintptr_t addr = (uintptr_t)offset;
    size_t padding = (alignment - (addr & (alignment - 1))) & (alignment - 1);

    if ((offset - buffer) + padding + size > capacity) {
      return nullptr;
    }

    offset += padding;
    void *result = offset;
    offset += size;

    return result;
  }

  size_t marker() { return buffer - offset; }

  void freeTo(size_t marker) { offset = buffer + marker; }

  void reset() { offset = buffer; }

  int expand(size_t size) {
    if (size <= capacity) {
      return 0;
    }

    capacity = size;
    size_t _offset = offset - buffer;

    void *temp = realloc(buffer, capacity);
    if (!temp) {
      return -1;
    }
    buffer = (unsigned char *)temp;
    offset = buffer + _offset;
    return 0;
  }

  ~StackAllocator() { free(buffer); }
};

template <typename T> class PoolAllocator {
  void **freePtr;
  T *buffer = nullptr;
  size_t capacity = 0;
  size_t slotsAvailable = 0;

public:
  PoolAllocator() = delete;
  PoolAllocator(const PoolAllocator<T> &other) = delete;

  PoolAllocator(size_t count) : capacity(count), slotsAvailable(count) {
    static_assert(sizeof(T) >= sizeof(void *),
                  "Pool Allocator Cannot Take A Type smaller than 8 bytes");

    buffer = (T *)malloc(count * sizeof(T));

    if (!buffer) {
      abort(); // out of memory
    }

    for (size_t i = 0; i < count - 1; i++) {
      *(T **)&buffer[i] = &buffer[i + 1];
    }

    *(T **)&buffer[count - 1] = nullptr;

    freePtr = buffer;
  }

  PoolAllocator(PoolAllocator &&allocator) {
    capacity = allocator.capacity;
    slotsAvailable = allocator.slotsAvailable;
    buffer = allocator.buffer;
    freePtr = allocator.freePtr;

    allocator.capacity = 0;
    allocator.slotsAvailable = 0;
    allocator.buffer = nullptr;
    allocator.freePtr = nullptr;
  }

  T *alloc() {
    if (!freePtr) {
      return nullptr;
    }

    T *temp = (T *)freePtr;
    freePtr = (void **)*freePtr;
    slotsAvailable--;
    return temp;
  }

  void free(T *ptr) {
    *(void **)ptr = freePtr;
    freePtr = ptr;
    slotsAvailable++;
    return;
  }

  bool isFull() { return freePtr; }

  size_t available() { return slotsAvailable; }

  ~PoolAllocator() { free(buffer); }
};

template <typename T> class Slice {
  T *ptr;
  size_t len;

public:
  Slice() = delete;
  Slice(T *_ptr, size_t _len) : ptr(_ptr), len(_len) {}

  T &operator[](size_t index) { return ptr[index]; }

  Result<T *> at(size_t index) {
    if (index >= len) [[unlikely]] {
      return Result<T *>{.error = "out of bounds", .ok = false};
    }

    return Result<T *>{.value = &ptr[index], .ok = true};
  }

  ~Slice() {
    ptr = nullptr;
    len = 0;
  }
};

} // namespace core
