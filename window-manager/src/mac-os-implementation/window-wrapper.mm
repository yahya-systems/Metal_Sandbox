#include "Metal/Metal.h"
#include "keycodes.h"
#include "tools/slots.h"

#include "wm/window-manager.h"
#include <AppKit/AppKit.h>
#import <Cocoa/Cocoa.h>
#include <Foundation/Foundation.h>
#include <objc/objc.h>
#ifdef WM_INCLUDE_METAL
#include <Metal/Metal.h>
#include <QuartzCore/CAMetalLayer.h>
#endif
#include <cstdint>

namespace WM {
// Event Manager decalarations
uint32_t getEventCount();
void clearEvents();
void pushEvent(const WM::Event *event);
// Pushes a window onto the destruction queue
void pushToDestructionQueue(NSWindow *window);

// Empties the destruction queue, setting all stored windows to nil
void emptyDestructionQueue();

Event eventQueue[MAX_EVENTS];
uint32_t index;
uint32_t iterator;
} // namespace WM

struct WindowCallbacks {
  bool (*windowCloseCallback)(uint32_t window);
  void (*windowResizeCallback)(uint32_t window, uint32_t width,
                               uint32_t height);
  void (*onFocus)(uint32_t window);
  void (*onLostFocus)(uint32_t window);
  void (*onMinimize)(uint32_t window);
  void (*onRestore)(uint32_t window);
  void (*onEnterFullscreen)(uint32_t window);
  void (*onExitFullscreen)(uint32_t window);
  void (*onMove)(uint32_t window, double x, double y);
};

struct WindowHandle {
  NSWindow *window;
  char title[128];
  uint32_t ID;
  WM::vec2<int32_t> dimensions;
  WM::vec2<double> position;
  bool isFocused;
  bool isMapped;
  bool isFullscreen;
  bool isMinimized;
  uint32_t monitorID;
  WM::vec2<int32_t> monitorDimensions;
  WindowCallbacks callbacks;
};

struct MacScreen {
  WM::Screen screen;
  NSScreen *nsScreen;
};

array<MacScreen, 32> screens;
uint32_t screenCount{};

bool appShouldClose = false;
uint32_t windowCount = 0;
slotMap<WindowHandle> map;

// events
bool keys[255] = {false};
WM::vec2<double> mouseCoordinates;
WM::vec2<double> mouseCordsDelta;
bool mouseRightDown = false;
bool mouseLeftDown = false;
bool cursorVisible = true;

int32_t resizeEventIndex = -1;

void closeOperations(uint32_t window) {
  WindowHandle *handle = map[window].unwrap();

  WM::Event event{.eventType = WM::WindowClose, .windowID = window};
  event.data.none = 0;
  WM::pushEvent(&event);
  windowCount--;
  map.remove(window);

  if (windowCount == 0) {
    appShouldClose = true;
  }
  WM::pushToDestructionQueue((handle->window));
}

void resizeOperations(uint32_t window) {
  WindowHandle *handle = map[window].unwrap();

  NSSize size = [handle->window frame].size;
  CGFloat backingScale = [handle->window backingScaleFactor];
  uint32_t width = size.width * backingScale;
  uint32_t height = size.height * backingScale;
  handle->dimensions.x = size.width;
  handle->dimensions.y = size.height;

  if (handle->callbacks.windowResizeCallback) {
    handle->callbacks.windowResizeCallback(window, width, height);
  }
  if (resizeEventIndex == -1) {
    WM::Event event = {.eventType = WM::WindowResize, .windowID = window};
    event.data.dimensions = {handle->dimensions.x, handle->dimensions.y};
    resizeEventIndex = WM::index;
    WM::pushEvent(&event);
  } else {
    WM::eventQueue[resizeEventIndex].data.dimensions = {handle->dimensions.x,
                                                        handle->dimensions.y};
  }
}

@interface WindowDelegate : NSObject <NSWindowDelegate>
@property(assign) struct WindowHandle *handle;
@end
@implementation WindowDelegate
- (BOOL)windowShouldClose:(NSNotification *)notification {
  WindowHandle *handle = map[self.handle->ID].unwrap();
  if (handle->callbacks.windowCloseCallback) {
    if (!handle->callbacks.windowCloseCallback(self.handle->ID)) {
      return NO;
    }
  }
  closeOperations(self.handle->ID);
  return YES;
}

- (void)windowDidResize:(NSNotification *)notification {
  resizeOperations(self.handle->ID);
}

// For Focus / Unfocus
- (void)windowDidBecomeKey:(NSNotification *)notification {
  WM::Event event{.eventType = WM::WindowFocus, .windowID = self.handle->ID};

  event.data.focused = true;

  WM::pushEvent(&event);
  if (self.handle->callbacks.onFocus) {
    self.handle->callbacks.onFocus(self.handle->ID);
  }
}

- (void)windowDidResignKey:(NSNotification *)notification {
  WM::Event event{.eventType = WM::WindowFocus, .windowID = self.handle->ID};

  event.data.focused = false;

  WM::pushEvent(&event);
  if (self.handle->callbacks.onLostFocus) {
    self.handle->callbacks.onLostFocus(self.handle->ID);
  }
}

- (void)windowDidMiniaturize:(NSNotification *)notification {
  WM::Event event{.eventType = WM::WindowMinimize, .windowID = self.handle->ID};

  event.data.minimized = true;
  WM::pushEvent(&event);
  if (self.handle->callbacks.onMinimize) {
    self.handle->callbacks.onMinimize(self.handle->ID);
  }
  self.handle->isMinimized = true;
}

- (void)windowDidDeminiaturize:(NSNotification *)notification {
  WM::Event event{.eventType = WM::WindowMinimize, .windowID = self.handle->ID};
  event.data.minimized = false;
  WM::pushEvent(&event);
  if (self.handle->callbacks.onRestore) {
    self.handle->callbacks.onRestore(self.handle->ID);
  }
  self.handle->isMinimized = false;
}

- (void)windowDidEnterFullScreen:(NSNotification *)notification {
  WM::Event event{.eventType = WM::WindowFullScreen,
                  .windowID = self.handle->ID};
  event.data.fullscreen = true;
  WM::pushEvent(&event);
  if (self.handle->callbacks.onEnterFullscreen) {
    self.handle->callbacks.onEnterFullscreen(self.handle->ID);
  }
  self.handle->isFullscreen = true;
}

- (void)windowDidExitFullScreen:(NSNotification *)notification {
  WM::Event event{.eventType = WM::WindowFullScreen,
                  .windowID = self.handle->ID};
  event.data.fullscreen = false;
  WM::pushEvent(&event);
  if (self.handle->callbacks.onExitFullscreen) {
    self.handle->callbacks.onExitFullscreen(self.handle->ID);
  }
  self.handle->isFullscreen = false;
}

- (void)windowDidMove:(NSNotification *)notification {
  NSWindow *window = [notification object];
  NSScreen *screen = [window screen] ?: [NSScreen mainScreen];

  NSRect windowFrame = [window frame];
  NSRect screenFrame = [screen frame];

  double x = windowFrame.origin.x - screenFrame.origin.x;

  double monitorTop = screenFrame.origin.y + screenFrame.size.height;
  double windowTop = windowFrame.origin.y + windowFrame.size.height;

  double y = monitorTop - windowTop;

  WM::Event event{.eventType = WM::WindowMove, .windowID = self.handle->ID};
  event.data.position = WM::vec2<double>{x, y};
  WM::pushEvent(&event);
  self.handle->position = {x, y};
  if (self.handle->callbacks.onMove) {
    self.handle->callbacks.onMove(self.handle->ID, x, y);
  }
}
@end

namespace WM {

void init() {
  [NSApplication sharedApplication];
  [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
  [NSApp finishLaunching];

  [NSApp activateIgnoringOtherApps:YES];

  NSArray<NSScreen *> *nsScreens = [NSScreen screens];

  for (NSScreen *nsScreen in nsScreens) {
    Screen screen;
    BOOL success = [nsScreen.localizedName getCString:screen.name
                                            maxLength:256
                                             encoding:NSUTF8StringEncoding];

    screen.depth = nsScreen.depth;
    screen.dimensions.x =
        nsScreen.frame.size.width * nsScreen.backingScaleFactor;
    screen.dimensions.y =
        nsScreen.frame.size.height * nsScreen.backingScaleFactor;

    screen.position.x = nsScreen.frame.origin.x;
    screen.position.y = nsScreen.frame.origin.y;

    screens[screenCount++] = {screen, nsScreen};
  }
}

Result<uint32_t> createWindow(const WindowCreateInfo &createInfo) {
  if (strlen(createInfo.title) >= 128) {
    return Result<uint32_t>{.error = "Title is longer than 128", .ok = false};
  }
  NSWindowStyleMask styleMask = NSWindowStyleMask(createInfo.styles);
  NSScreen *screen = [NSScreen mainScreen];
  // We allocate the window
  vec2<int32_t> offset = {};
  NSScreen *monitor;
  if (createInfo.monitor == -1) {
    offset.x = [NSScreen mainScreen].frame.origin.x;
    offset.y = [NSScreen mainScreen].frame.origin.y;
    monitor = [NSScreen mainScreen];
  } else {

    Result<MacScreen *> result = screens.at(createInfo.monitor);

    if (!result.ok) {
      return Result<uint32_t>{.error = "Invalid Monitor ID", .ok = false};
    }

    offset.x = result.value->nsScreen.frame.origin.x;
    offset.y = result.value->nsScreen.frame.origin.y;

    monitor = result.value->nsScreen;
  }

  NSWindow *window = [[NSWindow alloc]
      initWithContentRect:NSMakeRect(0, 0, createInfo.width,
                                     /* screen.frame.size.height - */
                                     createInfo.height)
                styleMask:styleMask
                  backing:NSBackingStoreBuffered
                    defer:NO];

  if (window == nil) {
    return Result<uint32_t>{.error = "failed to create window", .ok = false};
  }

  NSRect screenFrame = [monitor frame];

  NSPoint globalTopLeft =
      NSMakePoint(screenFrame.origin.x + (double)createInfo.xPosition,
                  screenFrame.origin.y + screenFrame.size.height -
                      (double)createInfo.yPosition);

  [window setFrameTopLeftPoint:globalTopLeft];

  // We set the window title
  if (createInfo.title) {
    [window setTitle:[NSString stringWithUTF8String:createInfo.title]];
  }
  [window setOpaque:YES];

  WindowHandle handleData = {
      .window = window,
      .dimensions = {(int32_t)createInfo.width, (int32_t)createInfo.height},
      .isFocused = true,
      .isMapped = true,
      .isFullscreen = false,
      .isMinimized = false,
      .monitorID = 0,
  };

  strcpy(handleData.title, createInfo.title);

  uint32_t windowIndex = map.insert(handleData);

  map[windowIndex].value->ID = windowIndex;
  windowCount++;

  NSScreen *nsscreen = [window screen] ?: [NSScreen mainScreen];

  int32_t screenHeight = [nsscreen frame].size.height;
  int32_t screenWidth = [nsscreen frame].size.width;

  handleData.monitorDimensions = {screenHeight, screenWidth};

  WindowDelegate *windowDelegate = [[WindowDelegate alloc] init];

  if (windowDelegate == nil) {
    return Result<uint32_t>{.error = "failed to initialize window delegate",
                            .ok = false};
  }
  [window setDelegate:windowDelegate];

  windowDelegate.handle = map[windowIndex].value;

  NSView *content_view = [[NSView alloc] initWithFrame:window.frame];

  if (createInfo.disableAnimations) {
    [window setAnimationBehavior:NSWindowAnimationBehaviorNone];
  }

  [NSAnimationContext endGrouping];
  [window setContentView:content_view];

  return Result<uint32_t>{.value = windowIndex, .ok = true};
}

void pollEvents() {
  @autoreleasepool {
    clearEvents();
    NSEvent *event;

    bool mouseMoved = false;
    vec2<double> mouseDelta = {0, 0};
    bool mouseScrolled = false;
    vec2<double> mouseScrollDelta = {0, 0};
    uint32_t window = 0;
    while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                       untilDate:[NSDate distantPast]
                                          inMode:NSDefaultRunLoopMode
                                         dequeue:YES])) {

      NSEventType type = [event type];
      WindowDelegate *delegate = (WindowDelegate *)[event.window delegate];

      switch (type) {
      case NSEventTypeKeyDown: {
        if (!delegate || !event.window) [[unlikely]]
          continue;
        uint32_t key = [event keyCode];
        NSEventModifierFlags flags = [event modifierFlags];
        uint32_t clickedFlags =
            flags & ((1ULL << 17) | (1ULL << 18) | (1ULL << 19) | (1ULL << 20));
        if (clickedFlags) {
          Event ev = {.eventType = KeyComb, .windowID = delegate.handle->ID};
          ev.data.combination.press.keyCode = key;
          ev.data.combination.modifierCombination = clickedFlags >> 17;
          BOOL success = [[event characters]
              getCString:ev.data.combination.press.osLayoutKey
               maxLength:8
                encoding:NSUTF8StringEncoding];

          pushEvent(&ev);
          break;
        }
        keys[key] = true;
        Event ev = {.eventType = KeyDown, .windowID = delegate.handle->ID};
        ev.data.key.keyCode = key;
        BOOL success = [[event characters] getCString:ev.data.key.osLayoutKey
                                            maxLength:8
                                             encoding:NSUTF8StringEncoding];
        pushEvent(&ev);
        break;
      }
      case NSEventTypeKeyUp: {
        if (!delegate || !event.window) [[unlikely]]
          continue;
        uint32_t key = [event keyCode];
        keys[key] = false;
        Event ev = {.eventType = KeyUp, .windowID = delegate.handle->ID};
        ev.data.key.keyCode = key;
        BOOL success = [[event characters] getCString:ev.data.key.osLayoutKey
                                            maxLength:8
                                             encoding:NSUTF8StringEncoding];
        pushEvent(&ev);
        break;
      }
      case NSEventTypeFlagsChanged: {
        if (!delegate || !event.window)
          continue;
        static NSEventModifierFlags previousFlagState;
        NSEventModifierFlags flags = [event modifierFlags];
        NSEventModifierFlags modifierKey = previousFlagState ^ flags;
        keys[KEY_CAPS_LOCK] = (flags & (1ULL << 16)) != 0; // Caps Lock
        keys[KEY_SHIFT] = (flags & (1ULL << 17)) != 0;     // Any Shift
        keys[KEY_CONTROL] = (flags & (1ULL << 18)) != 0;   // Any Control
        keys[KEY_OPTION] = (flags & (1ULL << 19)) != 0;    // Any Option (Alt)
        keys[KEY_COMMAND] = (flags & (1ULL << 20)) != 0;   // Any Command
        keys[KEY_FUNCTION] = (flags & (1ULL << 23)) != 0;  // Function (fn) key

        Event ev = {.windowID = delegate.handle->ID};
        // Shift
        if (modifierKey & (1ULL << 17)) {
          ev.eventType = (flags & (1ULL << 17)) ? KeyDown : KeyUp;
          ev.data.key.keyCode = KEY_SHIFT;
          pushEvent(&ev);
        }

        // Control
        if (modifierKey & (1ULL << 18)) {
          ev.eventType = (flags & (1ULL << 18)) ? KeyDown : KeyUp;
          ev.data.key.keyCode = KEY_CONTROL;
          pushEvent(&ev);
        }

        // Option / Alt
        if (modifierKey & (1ULL << 19)) {
          ev.eventType = (flags & (1ULL << 19)) ? KeyDown : KeyUp;
          ev.data.key.keyCode = KEY_OPTION;
          pushEvent(&ev);
        }

        // Command
        if (modifierKey & (1ULL << 20)) {
          ev.eventType = (flags & (1ULL << 20)) ? KeyDown : KeyUp;
          ev.data.key.keyCode = KEY_COMMAND;
          pushEvent(&ev);
        }

        // Function (fn)
        if (modifierKey & (1ULL << 23)) {
          ev.eventType = (flags & (1ULL << 23)) ? KeyDown : KeyUp;
          ev.data.key.keyCode = KEY_FUNCTION;
          pushEvent(&ev);
        }
        previousFlagState = flags;

        break;
      }
      case NSEventTypeMouseMoved: {
        if (!delegate || !event.window) [[unlikely]]
          continue;

        mouseMoved = true;
        NSPoint mouseCords = [event locationInWindow];
        double x = (double)mouseCords.x;
        double y = (double)mouseCords.y;
        mouseCoordinates.x = x;
        mouseCoordinates.y = y;

        double deltaX = [event deltaX];
        double deltaY = [event deltaY];
        mouseDelta.x += deltaX;
        mouseDelta.y += deltaY;
        window = delegate.handle->ID;
        break;
      }
      case NSEventTypeScrollWheel: {
        mouseScrolled = true;
        CGFloat mouseXdelta = event.scrollingDeltaX;
        CGFloat mouseYdelta = event.scrollingDeltaY;
        mouseScrollDelta.x += mouseXdelta;
        mouseScrollDelta.y += mouseYdelta;
        break;
      }
      case NSEventTypeRightMouseDown: {
        if (!delegate || !event.window) [[unlikely]]
          continue;
        mouseRightDown = true;

        Event ev = {.eventType = MouseDown, .windowID = delegate.handle->ID};
        ev.data.mouseButton = 1;
        pushEvent(&ev);
        break;
      }
      case NSEventTypeLeftMouseDown: {
        if (!delegate || !event.window)
          continue;
        mouseLeftDown = true;

        Event ev = {.eventType = MouseDown, .windowID = delegate.handle->ID};
        ev.data.mouseButton = 0;
        pushEvent(&ev);
        break;
      }
      case NSEventTypeOtherMouseDown: {
        if (!delegate || !event.window)
          continue;
        Event ev = {.eventType = MouseDown, .windowID = delegate.handle->ID};
        ev.data.mouseButton = event.buttonNumber;
        pushEvent(&ev);
        break;
      }
      case NSEventTypeRightMouseUp: {
        if (!delegate || !event.window) [[unlikely]]
          continue;
        mouseRightDown = false;
        Event ev = {.eventType = MouseUp, .windowID = delegate.handle->ID};
        ev.data.mouseButton = 1;
        pushEvent(&ev);
        break;
      }
      case NSEventTypeLeftMouseUp: {
        if (!delegate || !event.window) [[unlikely]]
          continue;
        mouseLeftDown = false;
        Event ev = {.eventType = MouseUp, .windowID = delegate.handle->ID};
        ev.data.mouseButton = 0;
        pushEvent(&ev);
        break;
      }
      case NSEventTypeOtherMouseUp: {
        if (!delegate || !event.window)
          continue;
        Event ev = {.eventType = MouseUp, .windowID = delegate.handle->ID};
        ev.data.mouseButton = event.buttonNumber;
        pushEvent(&ev);
        break;
      }
      default:
        break;
      }
      if (type != NSEventTypeKeyDown && type != NSEventTypeKeyUp) {
        [NSApp sendEvent:event];
      }
    }
    [NSApp updateWindows];
    if (mouseMoved) {
      Event event{.eventType = MouseMove, .windowID = window};
      event.data.mouseScrollDelta = mouseDelta;
      pushEvent(&event);
    }
    mouseCordsDelta = mouseDelta;
    if (mouseScrolled) {
      Event event{.eventType = MouseScroll, .windowID = window};
      event.data.mouseScrollDelta = mouseScrollDelta;
      pushEvent(&event);
    }
    emptyDestructionQueue();
  }
}

void *getCocoaWindow(uint32_t window) {
  WindowHandle *handle = map[window].unwrap();
  return handle->window;
}

void getSystemsScreens(Screen *buffer, uint32_t *_screenCount) {
  *_screenCount = screenCount;
  for (uint32_t i = 0; i < screenCount; i++) {
    buffer[i] = screens[i].screen;
  }
}

WindowInfo getWindowInfo(uint32_t window) {
  WindowHandle *handle = map[window].unwrap();
  WindowInfo info;
  info.title = handle->title;
  info.ID = window;
  info.width = handle->dimensions.x;
  info.height = handle->dimensions.y;
  return info;
}

#ifdef WM_INCLUDE_METAL

void *createMetalLayer(uint32_t window, void *device) {
  WindowHandle *handle = map[window].unwrap();

  CAMetalLayer *layer = [CAMetalLayer layer];
  layer.device = (__bridge id<MTLDevice>)device;
  layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
  uint32_t width = handle->dimensions.x * [handle->window backingScaleFactor];
  uint32_t height = handle->dimensions.y * [handle->window backingScaleFactor];
  layer.drawableSize = CGSizeMake(width, height);
  handle->window.contentView.wantsLayer = YES;
  handle->window.contentView.layer = layer;
  return layer;
}

#endif

double getBackingScaleFactor(uint32_t window) {
  WindowHandle *handle = map[window].unwrap();
  return [handle->window backingScaleFactor];
}

bool isKeyPressed(unsigned char key) { return keys[key]; }

vec2<double> getMousePosition() { return mouseCoordinates; }

vec2<double> getMousePositionDelta() { return mouseCordsDelta; }

bool isMouseRightDown() { return mouseRightDown; }

bool isMouseLeftDown() { return mouseLeftDown; }

// Changes Cursor visiblity
void setCursorVisible(bool visible) {
  if (visible == cursorVisible)
    return;
  if (visible) {
    [NSCursor unhide];
  } else {
    [NSCursor hide];
  }
  cursorVisible = visible;
}

static NSCursor *getCursorForType(CursorType type) {
  switch (type) {
  case CURSOR_ARROW:
    return [NSCursor arrowCursor];
  case CURSOR_CROSSHAIR:
    return [NSCursor crosshairCursor];
  case CURSOR_IBEAM:
    return [NSCursor IBeamCursor];
  case CURSOR_OPEN_HAND:
    return [NSCursor openHandCursor];
  case CURSOR_CLOSED_HAND:
    return [NSCursor closedHandCursor];
  case CURSOR_POINTING_HAND:
    return [NSCursor pointingHandCursor];
  default:
    return [NSCursor arrowCursor];
  }
}

void setCursorLook(CursorType type) { [getCursorForType(type) set]; }

void pushCursorLook(CursorType type) { [getCursorForType(type) push]; }

void popCursorLook() { [NSCursor pop]; }

void lockCursor() {
  CGAssociateMouseAndMouseCursorPosition(false);
  setCursorVisible(false);
}

void unlockCursor() {
  CGAssociateMouseAndMouseCursorPosition(true);
  setCursorVisible(true);
}

void mapWindow(uint32_t window) {
  WindowHandle *handle = map[window].unwrap();
  handle->isMapped = true;
  [handle->window makeKeyAndOrderFront:nil];
}

void unmapWindow(uint32_t window) {
  WindowHandle *handle = map[window].unwrap();
  handle->isMapped = false;
  [handle->window orderOut:nil];
}

void setWindowTitle(uint32_t window, const char *title) {
  WindowHandle *handle = map[window].unwrap();
  [handle->window setTitle:[NSString stringWithUTF8String:title]];
}

void closeWindow(uint32_t window) {
  WindowHandle *handle = map[window].unwrap();
  closeOperations(window);
}

void resizeWindow(uint32_t window, uint32_t width, uint32_t height) {
  WindowHandle *handle = map[window].unwrap();
  CGSize size = {(double)width, (double)height};
  CGPoint center = {0, 0};
  NSRect frameSize = {center, size};
  [handle->window setFrame:frameSize display:YES];
}

void focusWindow(uint32_t window) {
  WindowHandle *handle = map[window].unwrap();
  handle->isMinimized = false;

  [handle->window makeKeyAndOrderFront:nil];
}

void minimizeWindow(uint32_t window) {
  WindowHandle *handle = map[window].unwrap();
  handle->isMinimized = true;
  [handle->window miniaturize:nil];
}

void deminimizeWindow(uint32_t window) {
  WindowHandle *handle = map[window].unwrap();
  handle->isMinimized = false;
  [handle->window deminiaturize:nil];
}

void toggleFullscreenWindow(uint32_t window) {
  WindowHandle *handle = map[window].unwrap();
  handle->isFullscreen = !handle->isFullscreen;
  [handle->window toggleFullScreen:nil];
}

void setWindowMode(uint32_t window, WindowMode mode) {
  WindowHandle *handle = map[window].unwrap();
  switch (mode) {
  case Windowed:
    if (handle->isFullscreen)
      toggleFullscreenWindow(window);
    break;
  case Fullscreen:
    if (!handle->isFullscreen)
      toggleFullscreenWindow(window);
    break;
  }
}

void moveWindow(uint32_t window, double x, double y) {
  WindowHandle *handle = map[window].unwrap();
  NSScreen *screen = [handle->window screen] ?: [NSScreen mainScreen];
  double screenHeight = [screen frame].size.height;
  NSPoint point{x, screenHeight - y};
  [handle->window setFrameTopLeftPoint:point];
}

bool isMinimized(uint32_t window) {
  WindowHandle *handle = map[window].unwrap();
  return handle->isMinimized;
}

bool isFullscreen(uint32_t window) {
  WindowHandle *handle = map[window].unwrap();
  return handle->isFullscreen;
}

void setCloseCallback(uint32_t window,
                      bool (*windowCloseCallback)(uint32_t window)) {
  WindowHandle *handle = map[window].unwrap();
  handle->callbacks.windowCloseCallback = windowCloseCallback;
}

void setResizeCallback(uint32_t window,
                       void (*windowResizeCallback)(uint32_t window,
                                                    uint32_t width,
                                                    uint32_t height)) {
  WindowHandle *handle = map[window].unwrap();
  handle->callbacks.windowResizeCallback = windowResizeCallback;
}

void setMoveCallback(uint32_t window,
                     void (*onMove)(uint32_t window, double x, double y)) {
  WindowHandle *handle = map[window].unwrap();
  handle->callbacks.onMove = onMove;
}

void setFocusCallback(uint32_t window, void (*onFocus)(uint32_t window)) {
  WindowHandle *handle = map[window].unwrap();
  handle->callbacks.onFocus = onFocus;
}

void setLostFocusCallback(uint32_t window,
                          void (*onLostFocus)(uint32_t window)) {
  WindowHandle *handle = map[window].unwrap();
  handle->callbacks.onLostFocus = onLostFocus;
}

void setMinimizeCallback(uint32_t window, void (*onMinimize)(uint32_t window)) {
  WindowHandle *handle = map[window].unwrap();
  handle->callbacks.onMinimize = onMinimize;
}

void setRestoreCallback(uint32_t window, void (*onRestore)(uint32_t window)) {
  WindowHandle *handle = map[window].unwrap();
  handle->callbacks.onRestore = onRestore;
}

void setEnterFullscreenCallback(uint32_t window,
                                void (*onEnterFullscreen)(uint32_t window)) {
  WindowHandle *handle = map[window].unwrap();
  handle->callbacks.onEnterFullscreen = onEnterFullscreen;
}

void setExitFullscreenCallback(uint32_t window,
                               void (*onExitFullscreen)(uint32_t window)) {
  WindowHandle *handle = map[window].unwrap();
  handle->callbacks.onExitFullscreen = onExitFullscreen;
}

bool shouldClose() { return appShouldClose; }

void quit() {
  for (uint32_t i = 0; i < 64; i++) {
    if (map[i].ok) {
      closeOperations(map[i].unwrap()->ID);
    }
  }
  // None for now
}

// Destruction Queue
NSWindow *destructionQueue[4];
uint32_t queueIterator = 0;

void pushToDestructionQueue(NSWindow *window) {
  if (queueIterator < 4)
    destructionQueue[queueIterator++] = window;
}

void emptyDestructionQueue() {
  while (queueIterator > 0) {
    [destructionQueue[--queueIterator] setDelegate:nil];
    [destructionQueue[queueIterator] close];
    destructionQueue[queueIterator] = nil;
  }
}

// Event queue

uint32_t getEventCount() { return index; }

void pushEvent(const Event *event) {
  if (index >= MAX_EVENTS) {
    // none
  }
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
  resizeEventIndex = -1;
  index = 0;
  iterator = 0;
}

} // namespace WM
