#ifndef CURSOR_SYSTEM
#define CURSOR_SYSTEM

#include "../window-slots.h"
#include <xcb/xcb.h>
#include <xcb/xcb_cursor.h>

void initializeCursors(xcb_connection_t *connection, xcb_screen_t *_screen);
void setCursor(uint32_t cursor);
void pushCursor(xcb_cursor_t cursor);
void popCursor();
void cleanupCursors();
void hideCursor();
void showCursor();
void intlockCursor(xcb_window_t window);
void updateLock();
#endif
