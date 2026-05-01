
#include "../window-wrapper.h"
#include "atom/atom-handler.h"
#include "cursor/cursor.h"
#include "error-handler/error-handler.h"
#include "window-slots.h"
#include <cstdlib>
#include <stdio.h>
#include <string.h>
#ifdef WM_INCLUDE_VULKAN
#include <vulkan/vulkan.h> // Core Vulkan API
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_xcb.h> // XCB-specific surface extensionsinclude <xcb/xproto.h>
#endif
#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>
#include <xcb/xcb_cursor.h>
#include <xcb/xcb_keysyms.h>
#define MAX_WINDOWS 32
#define MAX_EVENTS 128

xcb_connection_t *x_connection;
xcb_key_symbols_t *syms;
xcb_screen_t *screen;

bool appShouldClose = false;
uint32_t windowCount = 0;

uint32_t checkResultCookie(xcb_void_cookie_t cookie) {
  xcb_generic_error_t *error = xcb_request_check(x_connection, cookie);
  if (error) [[unlikely]] {
    uint32_t errorCode = error->error_code;
    free(error);
    return errorCode;
  }
  return 0;
}
xcb_intern_atom_cookie_t active_cookie;
namespace WM {

xcb_atom_t wm_delete_window_atom;
xcb_atom_t wm_protocols_atom;
xcb_atom_t net_active_window_atom;
xcb_atom_t net_window_state_atom;
xcb_atom_t net_window_minimize_atom;
xcb_atom_t net_window_change_state_atom;
xcb_atom_t net_window_fullscreen_atom;
xcb_atom_t net_wm_state_atom;

void pushEvent(const Event *event);
void clearEvents();

vec2 mousePosition{};
bool leftMouseButtonDown = false;
bool rightMouseButtonDown = false;
bool middleMouseButtonDown = false;
bool cursorLocked = false;
bool keys[255] = {false};

#ifdef WM_INCLUDE_VULKAN
void *ptr = nullptr;
#endif // WM_INCLUDE_VULKAN

bool atomsHandled = false;

void init() {
  int screenNumber = -1;
  x_connection = xcb_connect(nullptr, &screenNumber);
  syms = xcb_key_symbols_alloc(x_connection);
  uint32_t errorCode = xcb_connection_has_error(x_connection);
  // If connection is null or has error, just die
  if (!x_connection || errorCode) [[unlikely]] {
    logError(WM_ERR_TYPE_FATAL, "XCB CONNECTION FAILED", nullptr);
  }
  xcb_screen_iterator_t iterator =
      xcb_setup_roots_iterator(xcb_get_setup(x_connection));

  for (int i = 0; i < screenNumber; i++) {
    xcb_screen_next(&iterator);
  }
  screen = iterator.data;
  setAppShouldClose(&appShouldClose);
  initializeCursors(x_connection, screen);

  const char *atom_names[] = {"_NET_ACTIVE_WINDOW",       "WM_PROTOCOLS",
                              "WM_DELETE_WINDOW",         "_NET_WM_STATE",
                              "_NET_WM_STATE_HIDDEN",     "WM_CHANGE_STATE",
                              "_NET_WM_STATE_FULLSCREEN", "WM_STATE"};
  uint8_t ifexists[8] = {0};
  submitAtoms(x_connection, sizeof(ifexists), atom_names, ifexists);
}

uint32_t createWindow(const WindowCreateInfo *createInfo) {
  xcb_window_t windowID = xcb_generate_id(x_connection);
  uint32_t mask = XCB_CW_EVENT_MASK;
  uint32_t value_mask[] = {
      XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS |
      XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION |
      XCB_EVENT_MASK_BUTTON_MOTION | XCB_EVENT_MASK_ENTER_WINDOW |
      XCB_EVENT_MASK_LEAVE_WINDOW | XCB_EVENT_MASK_KEY_PRESS |
      XCB_EVENT_MASK_KEY_RELEASE | XCB_EVENT_MASK_STRUCTURE_NOTIFY |
      XCB_EVENT_MASK_PROPERTY_CHANGE | XCB_EVENT_MASK_FOCUS_CHANGE};

  xcb_void_cookie_t result = xcb_create_window_checked(
      x_connection, XCB_COPY_FROM_PARENT, windowID, screen->root,
      createInfo->xPosition, createInfo->yPosition, createInfo->width,
      createInfo->height, 10, XCB_WINDOW_CLASS_INPUT_OUTPUT,
      screen->root_visual, mask, value_mask);

  if (!atomsHandled) {
    xcb_atom_t atoms[8];
    collectAtoms(atoms);
    net_active_window_atom = atoms[0];
    wm_protocols_atom = atoms[1];
    wm_delete_window_atom = atoms[2];
    net_window_state_atom = atoms[3];
    net_window_minimize_atom = atoms[4];
    net_window_change_state_atom = atoms[5];
    net_window_fullscreen_atom = atoms[6];
    net_wm_state_atom = atoms[7];
    atomsHandled = true;
  }

  xcb_generic_error_t *error = xcb_request_check(x_connection, result);

  if (error) [[unlikely]] {
    logErrorWithCode(WM_ERR_TYPE_FATAL, error->error_code,
                     "WINDOW CREATION FAILED", error);
  }
  xcb_change_property(x_connection, XCB_PROP_MODE_REPLACE, windowID,
                      XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
                      strlen(createInfo->title), createInfo->title);
  result = xcb_map_window_checked(x_connection, windowID);
  error = xcb_request_check(x_connection, result);
  if (error) [[unlikely]] {
    logErrorWithCode(WM_ERR_TYPE_FATAL, error->error_code,
                     "WINDOW MAPPING FAILED", error);
  }
  uint32_t window_index = getEmptySlotIndex();
  WindowHandle handle = {.title = createInfo->title,
                         .ID = window_index,
                         .window = windowID,
                         .width = createInfo->width,
                         .height = createInfo->height,
                         .mapped = true,
                         .callbacks = {}};
  ++windowCount;
  insert(&handle);
  xcb_change_property(x_connection, XCB_PROP_MODE_REPLACE, windowID,
                      wm_protocols_atom, XCB_ATOM_ATOM, 32, 1,
                      &wm_delete_window_atom);
  xcb_flush(x_connection);
  printf("window (%u) is %u\n", window_index, windowID);
  return window_index;
}

constexpr float scrollSensitivity = 1.0f;

void pollEvents() {
  xcb_generic_event_t *event;
  clearEvents();
  vec2 mouseDelta = vec2{};
  vec2 initialMousePosition = vec2{};
  uint32_t eventWindowID;
  bool mouseMoved = false;
  vec2 mouseScrollDelta = vec2{};
  bool mouseScrolled = false;
  while ((event = xcb_poll_for_event(x_connection))) {
    switch (event->response_type & ~0x80) {
    case XCB_CONFIGURE_NOTIFY: {
      xcb_configure_notify_event_t *ev = (xcb_configure_notify_event_t *)event;
      WindowHandle *handle = getHandle(ev->window);
      if (!handle)
        return;
      if (ev->height != handle->height || ev->width != handle->width) {
        if (handle->callbacks.windowResizeCallback) {
          handle->callbacks.windowResizeCallback(handle->ID, ev->width,
                                                 ev->height);
        }
        handle->width = ev->width;
        handle->height = ev->height;
        Event wm_ev;
        wm_ev.eventType = WindowResize;
        wm_ev.windowID = handle->ID;
        wm_ev.data.dimensions = {ev->width, ev->height};
        pushEvent(&wm_ev);
      }
      break;
    }
    case XCB_FOCUS_IN: {
      xcb_focus_in_event_t *ev = (xcb_focus_in_event_t *)event;
      WindowHandle *handle = getHandle(ev->event);
      if (!handle)
        return;
      Event wm_ev;
      wm_ev.eventType = WindowFocus;
      wm_ev.windowID = handle->ID;
      wm_ev.data.focused = true;
      pushEvent(&wm_ev);
      if (handle->callbacks.onFocus) {
        handle->callbacks.onFocus(handle->ID);
      }
      break;
    }
    case XCB_FOCUS_OUT: {
      xcb_focus_out_event_t *ev = (xcb_focus_out_event_t *)event;
      WindowHandle *handle = getHandle(ev->event);
      if (!handle)
        return;
      Event wm_ev;
      wm_ev.eventType = WindowFocus;
      wm_ev.windowID = handle->ID;
      wm_ev.data.focused = false;
      pushEvent(&wm_ev);
      if (handle->callbacks.onLostFocus) {
        handle->callbacks.onLostFocus(handle->ID);
      }
      break;
    }
    case XCB_BUTTON_PRESS: {
      xcb_button_press_event_t *ev = (xcb_button_press_event_t *)event;
      WindowHandle *handle = getHandle(ev->event);
      if (!handle)
        return;
      switch (ev->detail) {
      case 1: {
        leftMouseButtonDown = true;
        Event wm_ev{};
        wm_ev.eventType = MouseDown;
        wm_ev.windowID = handle->ID;
        wm_ev.data.mouseButton = 0;
        pushEvent(&wm_ev);
        break;
      }
      case 2: {
        middleMouseButtonDown = true;
        Event wm_ev{};
        wm_ev.eventType = MouseDown;
        wm_ev.windowID = handle->ID;
        wm_ev.data.mouseButton = 1;
        pushEvent(&wm_ev);
        break;
      }
      case 3: {
        rightMouseButtonDown = true;
        Event wm_ev{};
        wm_ev.eventType = MouseDown;
        wm_ev.windowID = handle->ID;
        wm_ev.data.mouseButton = 2;
        pushEvent(&wm_ev);
        break;
      }
      case 4: {
        mouseScrolled = true;
        eventWindowID = handle->ID;
        mouseScrollDelta.y += scrollSensitivity;
        break;
      }
      case 5: {
        mouseScrolled = true;
        eventWindowID = handle->ID;
        mouseScrollDelta.y -= scrollSensitivity;
        break;
      }
      case 6: {
        mouseScrolled = true;
        eventWindowID = handle->ID;
        mouseScrollDelta.x += scrollSensitivity;
        break;
      }
      case 7: {
        mouseScrolled = true;
        eventWindowID = handle->ID;
        mouseScrollDelta.x -= scrollSensitivity;
        break;
      }
      default:
        break;
      }
      break;
    }
    case XCB_BUTTON_RELEASE: {
      xcb_button_release_event_t *ev = (xcb_button_release_event_t *)event;
      WindowHandle *handle = getHandle(ev->event);
      if (!handle)
        return;
      switch (ev->detail) {
      case 1: {
        leftMouseButtonDown = true;
        Event wm_ev{};
        wm_ev.eventType = MouseUp;
        wm_ev.windowID = handle->ID;
        wm_ev.data.mouseButton = 0;
        pushEvent(&wm_ev);
        break;
      }
      case 2: {
        middleMouseButtonDown = true;
        Event wm_ev{};
        wm_ev.eventType = MouseUp;
        wm_ev.windowID = handle->ID;
        wm_ev.data.mouseButton = 1;
        pushEvent(&wm_ev);
        break;
      }
      case 3: {
        rightMouseButtonDown = true;
        Event wm_ev{};
        wm_ev.eventType = MouseUp;
        wm_ev.windowID = handle->ID;
        wm_ev.data.mouseButton = 2;
        pushEvent(&wm_ev);
        break;
      }
      default:
        break;
      }
      break;
    }
    case XCB_MOTION_NOTIFY: {
      xcb_motion_notify_event_t *ev = (xcb_motion_notify_event_t *)event;

      WindowHandle *handle = getHandle(ev->event);
      if (!handle)
        return;
      mousePosition.x = ev->event_x;
      mousePosition.y = ev->event_y;
      if (!mouseMoved) {
        initialMousePosition = mousePosition;
        mouseMoved = true;
      }
      eventWindowID = handle->ID;
      mouseDelta.x = mousePosition.x - initialMousePosition.x;
      mouseDelta.y = mousePosition.y - initialMousePosition.y;
      break;
    }
    case XCB_KEY_PRESS: {
      xcb_key_press_event_t *ev = (xcb_key_press_event_t *)event;
      WindowHandle *handle = getHandle(ev->event);
      if (!handle)
        return;
      uint32_t sym = xcb_key_press_lookup_keysym(syms, ev, 0);
      uint32_t target = sym;

      if (sym >= 65421 && sym <= 65481) {
        target = 130 + (sym - 65421);
      } else if (sym >= 65505 && sym <= 65518) {
        target = 200 + (sym - 65505);
      } else if (sym > 127) {
        break;
      }
      uint16_t combinationMask = ev->state & 77;
      if (combinationMask) {
        uint16_t ctrlaltMask = (combinationMask & 12) >> 1;
        uint16_t modMask = (combinationMask & 64) >> 3;
        uint16_t shiftMask = (combinationMask & 1);
        combinationMask = ctrlaltMask | modMask | shiftMask;
        Event wm_ev;
        wm_ev.eventType = KeyComb;
        wm_ev.windowID = handle->ID;
        wm_ev.data.combination.keyCode = target;
        wm_ev.data.combination.modifierCombination = combinationMask;
        pushEvent(&wm_ev);
        break;
      }
      keys[target] = true;
      Event wm_ev;
      wm_ev.eventType = KeyDown;
      wm_ev.windowID = handle->ID;
      wm_ev.data.keyCode = target;
      pushEvent(&wm_ev);
      break;
    }
    case XCB_KEY_RELEASE: {
      xcb_key_press_event_t *ev = (xcb_key_press_event_t *)event;
      uint32_t sym = xcb_key_press_lookup_keysym(syms, ev, 0);
      uint32_t target = sym;
      WindowHandle *handle = getHandle(ev->event);
      if (!handle)
        return;
      if (sym >= 65421 && sym <= 65481) {
        target = 130 + (sym - 65421);
      } else if (sym >= 65505 && sym <= 65518) {
        target = 200 + (sym - 65505);
      } else if (sym > 127) {
        break;
      }
      keys[target] = false;
      Event wm_ev;
      wm_ev.eventType = KeyUp;
      wm_ev.windowID = ev->event;
      wm_ev.data.keyCode = target;
      pushEvent(&wm_ev);
      break;
    }
    case XCB_PROPERTY_NOTIFY: {
      xcb_property_notify_event_t *ev = (xcb_property_notify_event_t *)event;
      WindowHandle *handle = getHandle(ev->window);
      if (!handle)
        return;

      // --- 1. MINIMIZE LOGIC ---
      // Using your variable "net_wm_state_atom" which holds "WM_STATE"
      if (ev->atom == net_wm_state_atom) {
        xcb_get_property_cookie_t cookie =
            xcb_get_property(x_connection, 0, ev->window, net_wm_state_atom,
                             XCB_GET_PROPERTY_TYPE_ANY, 0, 2);

        xcb_get_property_reply_t *reply =
            xcb_get_property_reply(x_connection, cookie, NULL);
        if (reply && xcb_get_property_value_length(reply) > 0) {
          uint32_t state = *(uint32_t *)xcb_get_property_value(reply);
          free(reply);

          if (state == 3 && !handle->minimized) { // 3 = Iconic (Minimized)
            handle->minimized = true;
            Event wm_ev;
            wm_ev.eventType = WindowMinimize;
            wm_ev.windowID = handle->ID;
            wm_ev.data.minimized = true;
            pushEvent(&wm_ev);
            if (handle->callbacks.onMinimize)
              handle->callbacks.onMinimize(handle->ID);
          } else if (state == 1 && handle->minimized) { // 1 = Normal
            handle->minimized = false;
            Event wm_ev;
            wm_ev.eventType = WindowMinimize;
            wm_ev.windowID = handle->ID;
            wm_ev.data.minimized = false;
            pushEvent(&wm_ev);
            if (handle->callbacks.onRestore)
              handle->callbacks.onRestore(handle->ID);
          }
        }
      }

      // --- 2. FULLSCREEN LOGIC ---
      // Using your variable "net_window_state_atom" which holds "_NET_WM_STATE"
      else if (ev->atom == net_window_state_atom) {
        xcb_get_property_cookie_t cookie =
            xcb_get_property(x_connection, 0, ev->window, net_window_state_atom,
                             XCB_ATOM_ATOM, 0, 32);

        xcb_get_property_reply_t *reply =
            xcb_get_property_reply(x_connection, cookie, NULL);
        if (reply && xcb_get_property_value_length(reply) > 0) {
          xcb_atom_t *atoms = (xcb_atom_t *)xcb_get_property_value(reply);
          int num_atoms =
              xcb_get_property_value_length(reply) / sizeof(xcb_atom_t);
          bool is_now_fullscreen = false;

          for (int i = 0; i < num_atoms; i++) {
            if (atoms[i] == net_window_fullscreen_atom) {
              is_now_fullscreen = true;
              break;
            }
          }

          if (handle->fullScreen != is_now_fullscreen) {
            handle->fullScreen = is_now_fullscreen;

            Event wm_ev;
            wm_ev.eventType = WindowFullScreen;
            wm_ev.windowID = handle->ID;
            wm_ev.data.fullscreen = handle->fullScreen;
            pushEvent(&wm_ev);

            if (handle->fullScreen) {
              if (handle->callbacks.onEnterFullscreen)
                handle->callbacks.onEnterFullscreen(handle->ID);
            } else {
              if (handle->callbacks.onExitFullscreen)
                handle->callbacks.onExitFullscreen(handle->ID);
            }
          }
          free(reply);
        }
      }
      break;
    }
    case XCB_CLIENT_MESSAGE: {
      xcb_client_message_event_t *ev = (xcb_client_message_event_t *)event;

      // Does this message match the 'Delete' atom we stored earlier?
      if (ev->data.data32[0] == wm_delete_window_atom) {
        closeWindow(getHandle(ev->window)->ID);
      }
      break;
    }

    case 0: {
      xcb_generic_error_t *err = (xcb_generic_error_t *)event;
      printf("X11 Error: %d\n", err->error_code);
      break;
    }
    default:
      break;
    }
    free(event);
  }
  if (mouseMoved) {
    Event wm_ev;
    wm_ev.eventType = MouseMove;
    wm_ev.windowID = eventWindowID;
    wm_ev.data.mouseMoveDelta = mouseDelta;
    pushEvent(&wm_ev);
  }
  if (mouseScrolled) {
    Event wm_ev;
    wm_ev.eventType = MouseScroll;
    wm_ev.windowID = eventWindowID;
    wm_ev.data.mouseScrollDelta = mouseScrollDelta;
    pushEvent(&wm_ev);
  }
}

#ifdef WM_INCLUDE_VULKAN
void getRequiredExtensions(const char ***data, uint32_t *count) {
  *count = 2;
  char **extensions = (char **)malloc(sizeof(char *) * *count);
  if (!extensions) {
    logError(WM_ERR_TYPE_FATAL,
             "FAILED TO ALLOCATE MEMORY FOR REQUIRED EXTENSIONS", nullptr);
  }
  extensions[0] = VK_KHR_SURFACE_EXTENSION_NAME;
  extensions[1] = VK_KHR_XCB_SURFACE_EXTENSION_NAME;
  *data = (const char **)extensions;
  ptr = extensions;
}
void createVkSurface(uint32_t window, VkInstance instance,
                     VkSurfaceKHR *surface) {
  VkXcbSurfaceCreateInfoKHR createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
  createInfo.window = querySlot(window)->window;
  createInfo.connection = x_connection;
  if (vkCreateXcbSurfaceKHR(instance, &createInfo, nullptr, surface) !=
      VK_SUCCESS) {
    logError(WM_ERR_TYPE_FATAL, "FAILED TO CREATE VULKAN SURFACE", nullptr);
  }
}
#endif // WM_INCLUDE_VULKAN

void setCursorLook(CursorType type) { setCursor(type); }

void pushCursorLook(CursorType type) { pushCursor(type); }

void popCursorLook() { popCursor(); }

void setCursorVisible(bool visible) {
  if (visible) {
    showCursor();
  } else {
    hideCursor();
  }
}

void lockCursor() {
  logError(WM_ERR_TYPE_FATAL, "FUCK YOU AND YOUR CURSOR LOCK", nullptr);
}

void unlockCursor() {
  logError(WM_ERR_TYPE_FATAL, "FUCK YOU AND YOUR CURSOR UNLOCK", nullptr);
}

bool isKeyPressed(uint8_t key) { return keys[key]; }

vec2 getMousePosition() { return mousePosition; }

bool isMouseLeftDown() { return leftMouseButtonDown; }

bool isMouseMiddleDown() { return middleMouseButtonDown; }

bool isMouseRightDown() { return rightMouseButtonDown; }

void closeWindow(uint32_t window) {
  WindowHandle *handle = querySlot(window);
  if (handle->callbacks.windowCloseCallback) {
    if (!handle->callbacks.windowCloseCallback(window))
      return;
  }
  xcb_destroy_window(x_connection, handle->window);
  deleteSlot(window);
  --windowCount;
  if (windowCount == 0) {
    appShouldClose = true;
  }
  xcb_flush(x_connection);
}

WindowInfo queryWindowInfo(uint32_t window) {
  WindowHandle *handle = querySlot(window);
  uint32_t win = handle->window;
  uint32_t id = handle->ID;
  // 1. Send the request (The "Cookie")
  xcb_get_geometry_cookie_t cookie = xcb_get_geometry(x_connection, win);

  // 2. Wait for the reply
  xcb_generic_error_t *error = nullptr;
  xcb_get_geometry_reply_t *reply =
      xcb_get_geometry_reply(x_connection, cookie, &error);

  if (error) {
    logWindowError(WM_ERR_TYPE_FATAL, error->error_code, id,
                   "GEOMETRY FETCH FAILED", error);
  }
  WindowInfo info;
  info.title = handle->title;
  info.ID = id;
  info.xCoordinate = reply->x;
  info.yCoordinate = reply->y;
  info.width = reply->width;
  info.height = reply->height;
  return info;
}

void moveWindow(uint32_t window, double x, double y) {
  xcb_window_t windowID = querySlot(window)->window;
  uint32_t values[] = {static_cast<uint32_t>(x), static_cast<uint32_t>(y)};
  xcb_configure_window(x_connection, windowID,
                       XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
  xcb_flush(x_connection);
}

void resizeWindow(uint32_t window, uint32_t width, uint32_t height) {
  WindowHandle *handle = querySlot(window);
  xcb_window_t windowID = handle->window;
  uint32_t values[] = {width, height};
  xcb_configure_window(x_connection, windowID,
                       XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                       values);
  xcb_flush(x_connection);
  if (handle->callbacks.windowResizeCallback) {
    handle->callbacks.windowResizeCallback(window, width, height);
  }
}

void mapWindow(uint32_t window) {
  WindowHandle *handle = querySlot(window);
  if (handle->mapped) {
    logError(WM_ERR_TYPE_RECOVERABLE, "WINDOW ALREADY MAPPED", nullptr);
    return;
  }
  uint32_t windowID = handle->window;
  uint32_t ID = handle->ID;
  xcb_void_cookie_t result = xcb_map_window_checked(x_connection, windowID);
  uint32_t errorCode = checkResultCookie(result);
  if (errorCode != 0) [[unlikely]] {
    logWindowError(WM_ERR_TYPE_WARNING, errorCode, ID, "MAPPING FAILED",
                   nullptr);
  }
  handle->mapped = true;
  xcb_flush(x_connection);
}

void unmapWindow(uint32_t window) {
  WindowHandle *handle = querySlot(window);
  if (!handle->mapped) {
    logError(WM_ERR_TYPE_RECOVERABLE, "WINDOW ALREADY UNMAPPED", nullptr);
    return;
  }
  uint32_t windowID = handle->window;
  uint32_t ID = handle->ID;
  xcb_void_cookie_t result = xcb_unmap_window_checked(x_connection, windowID);
  uint32_t errorCode = checkResultCookie(result);
  if (errorCode != 0) [[unlikely]] {
    logWindowError(WM_ERR_TYPE_WARNING, errorCode, ID, "UNMAPPING FAILED",
                   nullptr);
  }
  handle->mapped = false;
  xcb_flush(x_connection);
}

void focusWindow(uint32_t window) {
  WindowHandle *handle = querySlot(window);

  xcb_client_message_event_t ev = {};
  ev.response_type = XCB_CLIENT_MESSAGE;
  ev.format = 32;
  ev.window = handle->window;
  ev.type = net_active_window_atom; // You need to intern this atom!
  ev.data.data32[0] = 1;            // 1 = From an application
  ev.data.data32[1] = XCB_CURRENT_TIME;
  ev.data.data32[2] = 0; // Currently focused window (0 is fine)

  xcb_send_event(x_connection, false, screen->root,
                 XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
                     XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY,
                 (const char *)&ev);
  xcb_flush(x_connection);
}

bool isMinimized(uint32_t window) { return querySlot(window)->minimized; }

bool isFullscreen(uint32_t window) { return querySlot(window)->fullScreen; }

void minimizeWindow(uint32_t window) {
  WindowHandle *handle = querySlot(window);

  if (handle->minimized)
    return;
  xcb_client_message_event_t ev{};
  ev.response_type = XCB_CLIENT_MESSAGE;
  ev.format = 32;
  ev.window = handle->window;
  ev.type = net_window_change_state_atom;
  ev.data.data32[0] = 3;
  xcb_send_event(x_connection, false, screen->root,
                 XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
                     XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY,
                 (const char *)&ev);
  handle->minimized = true;
  xcb_flush(x_connection);
}

void deminimizeWindow(uint32_t window) {
  WindowHandle *handle = querySlot(window);
  if (!handle->minimized)
    return;

  focusWindow(window);
  handle->minimized = false;
}

void toggleFullscreenWindow(uint32_t window) {
  WindowHandle *handle = querySlot(window);
  handle->fullScreen = !handle->fullScreen;
  xcb_client_message_event_t ev = {};
  ev.response_type = XCB_CLIENT_MESSAGE;
  ev.format = 32;
  ev.window = handle->window;
  ev.type = net_window_state_atom; // You need to intern this atom!
  ev.data.data32[0] = handle->fullScreen ? 1 : 0; // 1 = From an application
  ev.data.data32[1] = net_window_fullscreen_atom;
  ev.data.data32[2] = 0; // Currently focused window (0 is fine)
  ev.data.data32[3] = 1; // Currently focused window (0 is fine)

  xcb_send_event(x_connection, false, screen->root,
                 XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
                     XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY,
                 (const char *)&ev);
  xcb_flush(x_connection);
}

void setCloseCallback(uint32_t window, bool (*windowCloseCallback)(uint32_t)) {
  WindowHandle *handle = querySlot(window);
  handle->callbacks.windowCloseCallback = windowCloseCallback;
}

void setResizeCallback(uint32_t window,
                       void (*windowResizeCallback)(uint32_t window,
                                                    uint32_t width,
                                                    uint32_t height)) {
  WindowHandle *handle = querySlot(window);
  handle->callbacks.windowResizeCallback = windowResizeCallback;
}

void setFocusCallback(uint32_t window, void (*onFocus)(uint32_t)) {
  WindowHandle *handle = querySlot(window);
  handle->callbacks.onFocus = onFocus;
}

void setLostFocusCallback(uint32_t window, void (*onLostFocus)(uint32_t)) {
  WindowHandle *handle = querySlot(window);
  handle->callbacks.onLostFocus = onLostFocus;
}

void setEnterFullscreenCallback(uint32_t window,
                                void (*onEnterFullscreen)(uint32_t)) {
  WindowHandle *handle = querySlot(window);
  handle->callbacks.onEnterFullscreen = onEnterFullscreen;
}

void setExitFullscreenCallback(uint32_t window,
                               void (*onExitFullscreen)(uint32_t)) {
  WindowHandle *handle = querySlot(window);
  handle->callbacks.onExitFullscreen = onExitFullscreen;
}

bool shouldClose() { return appShouldClose; }

void quit() {
#ifdef WM_INCLUDE_VULKAN
  if (ptr) {
    free(ptr);
  }
#endif // WM_INCLUDE_VULKAN
  cleanupCursors();
  xcb_key_symbols_free(syms); // Free keysym table
  xcb_disconnect(x_connection);
}

// Event system
Event eventQueue[MAX_EVENTS];
uint32_t index;
uint32_t iterator;

uint32_t getEventCount() { return index; }

void crashWithMessage(const char *message) {
  fprintf(stderr, "Fatal : %s\n", message);
  exit(-1);
}

void pushEvent(const Event *event) {
  if (index >= MAX_EVENTS)
    crashWithMessage("Event Overflow!");
  eventQueue[index] = *event;
  index++;
}

bool getNextEvent(Event *event) {
  if (iterator >= index)
    return false;
  *event = eventQueue[iterator++];
  return true;
}

void clearEvents() {
  index = 0;
  iterator = 0;
}

} // namespace WM
