#pragma once
#include <cstdlib>
template <typename T> struct Result {
  union {
    T value;
    const char *error = nullptr;
  };
  bool ok;
  T &unwrap() {
    if (!ok) [[unlikely]] {
      abort();
    }
    return value;
  }
};
