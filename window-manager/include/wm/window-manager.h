#pragma once
#ifdef PLATFORM_LINUX
#include "../../src/linux-implementation/keycodes.h"
#endif
#ifdef PLATFORM_MACOS
#include "../../src/mac-os-implementation/keycodes.h"
#define WM_INCLUDE_METAL
#endif

#include "../../src/Result.hpp"
#include <cstdlib>
#include <stdint.h>

namespace WM {

#define MAX_EVENTS 128

template <typename T> struct vec2 {
  T x, y;
};

enum ModifierBits : uint64_t {
  MOD_SHIFT = 1ULL << 0,
  MOD_CTRL = 1ULL << 1,
  MOD_ALT = 1ULL << 2,
  MOD_SUPER = 1ULL << 3,
};

enum CursorType {
  CURSOR_ARROW,
  CURSOR_IBEAM,
  CURSOR_CROSSHAIR,
  CURSOR_CLOSED_HAND,
  CURSOR_OPEN_HAND,
  CURSOR_POINTING_HAND,
};

enum EventType : uint8_t {
  WindowClose,
  WindowResize,
  WindowFocus,
  WindowMinimize,
  WindowFullScreen,
  WindowMove,
  KeyDown,
  KeyUp,
  KeyRepeat,
  KeyComb,
  MouseMove,
  MouseDown,
  MouseUp,
  MouseScroll,
};

enum WindowMode {
  Windowed,
  Fullscreen,
};

struct keyPress {
  uint32_t keyCode;
  char osLayoutKey[8];
};

struct keyCombination {
  keyPress press;
  uint32_t modifierCombination;
};

struct Event {
  EventType eventType;
  uint32_t windowID;
  union {
    uint8_t none;
    vec2<int32_t> dimensions;
    bool focused;
    bool minimized;
    bool fullscreen;
    vec2<double> position;
    keyPress key;
    keyCombination combination;
    vec2<double> mouseMoveDelta;
    uint32_t mouseButton;
    vec2<double> mouseScrollDelta;
  } data;
};

bool getNextEvent(Event *event);

struct Screen {
  char name[256];
  uint32_t depth;
  vec2<int32_t> position;
  vec2<int32_t> dimensions;
};

enum MyWindowStyles : uint64_t {
  // makes the requested window borderless.
  StyleBorderless = 0,
  // adds a top bar that contains a title (required for closable, resizable,
  // miniaturizable)
  StyleTitled = 1 << 0,
  // adds the red x button to close the window.
  StyleClosable = 1 << 1,
  // adds teh yellow minimize button to minimize the window.
  StyleMiniaturizable = 1 << 2,
  // makes the window resizable.
  StyleResizable = 1 << 3,

  // Modern macOS Styles (Requires OS X 10.10+)
  // taller, cleaner title bar.
  StyleUnifiedTitleAndToolbar = 1 << 12,
  // window can enter "Mac Fullscreen" mode.
  StyleFullScreen = 1 << 14,
  // window can enter "Mac Fullscreen" mode
  StyleFullSizeContentView = 1 << 15,
  // content extends behind the title bar

  // Utility Styles
  // thinner title bar (for panels/toolbars)
  StyleUtilityWindow = 1 << 4,
  // specialized modal appearance
  StyleDocModalWindow = 1 << 6,
  // window doesn't take focus when clicked
  StyleNonactivatingPanel = 1 << 7,
  // dark, translucent "Heads Up Display" look
  StyleHudWindow = 1 << 13
};

// Information struct required by create window.
struct WindowCreateInfo {
  const char *title = "Untitled";
  int32_t monitor = -1;
  uint64_t styles = StyleClosable | StyleTitled | StyleResizable;
  uint32_t width = 800, height = 600;
  uint32_t xPosition = 0, yPosition = 0;
  bool disableAnimations = true;
};

struct WindowInfo {
  const char *title;
  uint32_t ID;
  double xCoordinate, yCoordinate;
  uint32_t width, height;
};

// Initiates Window Manager by requesting MAC OS for an application interface.
void init();

// Creates a window with properties from WindowCreateInfo (By default it's
// invisible, it has to be mapped by WM::mapWindow(window))
Result<uint32_t> createWindow(const WindowCreateInfo &createInfo);

// Returns a pointer to an internal buffer containing all systems screens
void getSystemsScreens(Screen *buffer, uint32_t *_screenCount);

// Flushes MAC OS events
void pollEvents();

// Closes a specific window, it calls the close callback (if defined)
void closeWindow(uint32_t window);

// Resizes the window
void resizeWindow(uint32_t window, uint32_t width, uint32_t height);

// Focuses on the window
void focusWindow(uint32_t window);

// Miniaturizes the window
void minimizeWindow(uint32_t window);

// Deminiaturizes the window
void deminimizeWindow(uint32_t window);

// Maps a window (makes it visible)
void mapWindow(uint32_t window);

// Unmaps a window (makes it invisible)
void unmapWindow(uint32_t window);

// Changes the window's tilte
void setWindowTitle(uint32_t window, const char *title);

/**
 * Toggles Fullscreen mode.
 */
void toggleFullscreenWindow(uint32_t window);

// Switches window state between fullscreen, and windowed
void setWindowMode(uint32_t window, WindowMode mode);

/**
 * Moves the window's bottom-left corner to the specified coordinates.
 */
void moveWindow(uint32_t window, double x, double y);

// Returns true if the window is minimized
bool isMinimized(uint32_t window);

// Returns true if the window is full Screen
bool isFullscreen(uint32_t window);

// Returns the current position of the mouse
vec2<double> getMousePosition();

// Returns the delta of the mouse position relative to the last frame
vec2<double> getMousePositionDelta();

// Return if current mouse right button is pressed
bool isMouseRightDown();

// Returns if the current left button is pressed
bool isMouseLeftDown();

// Returns if the middle mouse button is pressed
bool isMouseMiddleDown();

// Checks if a key is pressed
bool isKeyPressed(uint8_t key);

// Sets the cursor to invisible
void setCursorVisible(bool visible);

// Sets the cursor look
void setCursorLook(CursorType type);

// Pushes the cursor to the cursor stack (DO NOT MIX WITH SET CURSOR LOOK)
void pushCursorLook(CursorType type);

// Pops the cursor of the cursor stack
void popCursorLook();

// Locks the cursor
void lockCursor();

// Unlocks the cursor
void unlockCursor();

// Returns a struct that contains some important data about the window
WindowInfo getWindowInfo(uint32_t window);

// --- Window State Callbacks (Void) ---
void setCloseCallback(uint32_t window,
                      bool (*windowCloseCallback)(uint32_t window));
void setFocusCallback(uint32_t window, void (*onFocus)(uint32_t window));
void setLostFocusCallback(uint32_t window,
                          void (*onLostFocus)(uint32_t window));
void setMinimizeCallback(uint32_t window, void (*onMinimize)(uint32_t window));
void setRestoreCallback(uint32_t window, void (*onRestore)(uint32_t window));
void setEnterFullscreenCallback(uint32_t window,
                                void (*onEnterFullscreen)(uint32_t window));
void setExitFullscreenCallback(uint32_t window,
                               void (*onExitFullscreen)(uint32_t window));

// --- Window Geometry Callbacks (Data) ---
void setMoveCallback(uint32_t window,
                     void (*onMove)(uint32_t window, double x, double y));
// window
void setResizeCallback(uint32_t window,
                       void (*windowResizeCallback)(uint32_t window,
                                                    uint32_t width,
                                                    uint32_t height));

bool shouldClose();

#ifdef PLATFORM_MACOS
double getBackingScaleFactor(uint32_t window);
#ifdef WM_INCLUDE_METAL
void *createMetalLayer(uint32_t window, void *device);
#endif
#endif

#ifdef PLATFORM_LINUX

void getRequiredExtensions(const char ***data, uint32_t *count);

void createVkSurface(uint32_t window, VkInstance instance,
                     VkSurfaceKHR *surface);

#endif

// Deallocates resources, (mandatory to call this at the very end of the
// program)
void quit();
} // namespace WM
