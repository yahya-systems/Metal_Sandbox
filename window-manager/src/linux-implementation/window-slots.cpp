#include "window-slots.h"
#include <cstdint>
#define MAX_WINDOWS 32

WindowHandle handles[32];
uint32_t slot_map = 0;
bool firstOccupied = false;

uint32_t insert(const WindowHandle *handle) {
  if (slot_map == ~UINT32_C(0)) [[unlikely]]
    crashWithMessage("MAX IS 32 WINDOWS!");
  uint32_t available_slot = __builtin_ctz(~slot_map);
  if (!firstOccupied) {
    available_slot = 0;
    firstOccupied = true;
  }
  handles[available_slot] = *handle;
  slot_map |= (UINT32_C(1) << available_slot);
  return available_slot;
}
bool slotOccupied(uint32_t slot) {
  if (slot >= 32) [[unlikely]]
    crashWithMessage("SLOT INDEX OUT OF BOUNDS!");
  if (slot == 0)
    return firstOccupied;
  return slot_map & (UINT32_C(1) << slot);
}

WindowHandle *querySlot(uint32_t slot) {
  if (!slotOccupied(slot)) [[unlikely]]
    crashWithMessage("SLOT IS NOT OCCUPIED!");
  return &handles[slot];
}

uint32_t getEmptySlotIndex() {
  if (slot_map == ~UINT32_C(0)) [[unlikely]]
    crashWithMessage("THERE ARE NO EMPTY SLOTS");
  if (!firstOccupied)
    return 0;
  return __builtin_ctz(~slot_map);
}

void deleteSlot(uint32_t slot) {
  if (!slotOccupied(slot)) [[unlikely]]
    crashWithMessage("SLOT IS NOT OCCUPIED!");
  if (slot == 0) {
    firstOccupied = false;
    return;
  }
  slot_map = slot_map & ~(UINT32_C(1) << slot);
}

WindowHandle *getHandle(xcb_window_t window) {
  uint32_t result = 31 - __builtin_clz(slot_map);
  for (uint32_t i = 0; i <= result; i++) {
    if (handles[i].window == window && slotOccupied(i)) {
      return &handles[i];
    }
  }
  return nullptr;
}
