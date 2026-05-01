#include "cursor.h"
#include "../error-handler/error-handler.h"
#include <xcb/xcb.h>
#include <xcb/xproto.h>

#define CURSOR_COUNT 7

xcb_connection_t *xconnection;
xcb_screen_t *xscreen;

xcb_cursor_t cursors[CURSOR_COUNT] = {XCB_NONE};
bool cursorsOnline = true;
xcb_cursor_context_t *cursor_ctx;

xcb_cursor_t cursorStack[CURSOR_COUNT] = {XCB_NONE};
uint32_t currentCursor;

xcb_window_t windows[32] = {XCB_NONE};
uint32_t windowIterator = 0;

bool hidden = false;
bool cursorLocked = false;
xcb_window_t lockedWindow;

inline void updateDisplayCursor();

void initializeCursors(xcb_connection_t *connection, xcb_screen_t *_screen) {
  xconnection = connection;
  xscreen = _screen;
  // This looks at the user's ~/.Xresources or XDG theme settings
  if (xcb_cursor_context_new(connection, xscreen, &cursor_ctx) < 0)
      [[unlikely]] {
    logError(WM_ERR_TYPE_RECOVERABLE, "FAILED TO LOAD XCB_CURSOR CONTEXT",
             nullptr);
    cursorsOnline = false;
    return;
  }

  // Modern names that match MacOS/Windows/Linux themes
  cursors[0] = xcb_cursor_load_cursor(cursor_ctx, "left_ptr");
  cursors[1] = xcb_cursor_load_cursor(cursor_ctx, "xterm");
  cursors[2] = xcb_cursor_load_cursor(cursor_ctx, "crosshair");
  cursors[3] = xcb_cursor_load_cursor(cursor_ctx, "grabbing");
  cursors[4] = xcb_cursor_load_cursor(cursor_ctx, "grab");
  cursors[5] = xcb_cursor_load_cursor(cursor_ctx, "pointer");

  // Invisible cursor
  // -------------------------------------------------------------------
  xcb_pixmap_t cursor_pixmap = xcb_generate_id(connection);
  xcb_cursor_t invisible_cursor = xcb_generate_id(connection);

  // 2. Create a 1x1 pixmap with a depth of 1 bit
  // 'window' is your xcb_window_t handle
  xcb_create_pixmap(connection, 1, cursor_pixmap, _screen->root, 1, 1);

  // 3. Create the cursor using the empty pixmap as both source and mask
  // The RGB values (0,0,0) don't matter because the pixmap is empty
  xcb_create_cursor(connection, invisible_cursor, cursor_pixmap, cursor_pixmap,
                    0, 0, 0, // Foreground RGB
                    0, 0, 0, // Background RGB
                    0, 0     // Hotspot X, Y
  );
  // -------------------------------------------------------------------
  cursors[6] = invisible_cursor;

  cursorStack[0] = cursors[0];
}

void setCursor(uint32_t cursor) {
  if (hidden)
    return;
  if (cursor >= CURSOR_COUNT) [[unlikely]] {
    logError(WM_ERR_TYPE_RECOVERABLE, "CURSOR DOES NOT EXIST", nullptr);
    return;
  }
  cursorStack[currentCursor] = cursors[cursor];
  updateDisplayCursor();
}

void pushCursor(uint32_t cursor) {
  if (hidden)
    return;
  if (currentCursor == CURSOR_COUNT - 1) [[unlikely]] {
    logError(WM_ERR_TYPE_RECOVERABLE, "CURSOR STACK IS FULL", nullptr);
    return;
  }
  cursorStack[++currentCursor] = cursors[cursor];
  updateDisplayCursor();
}

void popCursor() {
  if (hidden)
    return;
  if (currentCursor == 0) [[unlikely]] {
    logError(WM_ERR_TYPE_RECOVERABLE, "CANNOT POP FIRST CURSOR", nullptr);
    return;
  }
  currentCursor--;
  updateDisplayCursor();
}

xcb_cursor_t tempCursor;
void hideCursor() {
  tempCursor = cursorStack[currentCursor];
  setCursor(6);
  hidden = true;
}

void showCursor() {
  cursorStack[currentCursor] = tempCursor;
  updateDisplayCursor();
  hidden = false;
}

void intlockCursor(uint32_t window) {
  cursorLocked = true;
  lockedWindow = window;
  hideCursor();
}

void intUnlockCursor() {
  cursorLocked = false;
  showCursor();
}

void centerCursor(xcb_window_t window, uint16_t width, uint16_t height) {
  // xcb_warp_pointer(connection, src_w, dest_w, src_x, src_y, src_w, src_h,
  // dest_x, dest_y)
  xcb_warp_pointer(xconnection,
                   XCB_NONE,   // We aren't warping relative to another window
                   window,     // The window to warp into
                   0, 0, 0, 0, // Source offsets/bounds (ignore)
                   width / 2,  // Target X (center)
                   height / 2  // Target Y (center)
  );

  // This is vital. Without the flush, the command sits in your local
  // buffer and the mouse won't actually move until the next poll.
  xcb_flush(xconnection);
}

void updateLock() {
  WindowHandle *handle = querySlot(lockedWindow);
  centerCursor(handle->window, handle->width, handle->height);
}

inline void updateDisplayCursor() {
  for (size_t i = 0; i < 32; ++i) {
    if (slotOccupied(i)) {
      WindowHandle *handle = querySlot(i);
      xcb_change_window_attributes(xconnection, handle->window, XCB_CW_CURSOR,
                                   &cursorStack[currentCursor]);
    }
  }
  xcb_flush(xconnection);
}

void cleanupCursors() {
  for (size_t i = 0; i < 6; i++) {
    if (cursors[i] != XCB_NONE) {
      xcb_free_cursor(xconnection, cursors[i]);
    }
  }
}
