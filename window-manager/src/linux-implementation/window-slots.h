#pragma once

#include "../window-wrapper.h"
#include <cstdint>
#include <cstdio>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <xcb/xcb.h>

inline void crashWithMessage(const char *message) {
  fprintf(stderr, "Fatal : %s\n", message);
  exit(-1);
}

struct WindowCallbacks {
  bool (*windowCloseCallback)(uint32_t window) = nullptr;
  void (*windowResizeCallback)(uint32_t window, uint32_t width,
                               uint32_t height) = nullptr;
  void (*onFocus)(uint32_t window) = nullptr;
  void (*onLostFocus)(uint32_t window) = nullptr;
  void (*onMinimize)(uint32_t window) = nullptr;
  void (*onRestore)(uint32_t window) = nullptr;
  void (*onEnterFullscreen)(uint32_t window) = nullptr;
  void (*onExitFullscreen)(uint32_t window) = nullptr;
  void (*onMove)(double x, double y, uint32_t window) = nullptr;
};

struct WindowHandle {
  const char *title;
  uint32_t ID;
  xcb_window_t window;
  uint32_t width, height;
  bool mapped = true;
  bool fullScreen = false;
  bool minimized = false;
  WindowCallbacks callbacks;
};

uint32_t insert(const WindowHandle *handle);
bool slotOccupied(uint32_t slot);
WindowHandle *querySlot(uint32_t slot);
uint32_t getEmptySlotIndex();
void deleteSlot(uint32_t slot);
WindowHandle *getHandle(xcb_window_t window);
