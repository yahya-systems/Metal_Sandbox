# WM API Reference

This document describes the public API in `include/wm/window-manager.h`.

```cpp
#include <wm/window-manager.h>
```

All functions and types are in the `WM::` namespace (except `Result<T>`, which
is a small helper type pulled in from `src/Result.hpp`).

## Initialization & lifecycle

### `void WM::init()`
Initializes the window manager. Call once at program start.

### `void WM::quit()`
Deallocates resources. Call once at program exit.

### `bool WM::shouldClose()`
Returns `true` when the app should exit (e.g., all windows have been closed).

## Window creation

### `struct WM::WindowCreateInfo`

Fields (defaults shown in the header):

- `title` (`"Untitled"`)
- `monitor` (`-1` = main screen)
- `styles` (`WM::StyleClosable | WM::StyleTitled | WM::StyleResizable`)
- `width`, `height` (`800x600`)
- `xPosition`, `yPosition` (`0,0`)
- `disableAnimations` (`true`)

### `Result<uint32_t> WM::createWindow(const WindowCreateInfo& createInfo)`
Creates a window and returns a handle (ID). On success `result.ok == true` and
`result.value` is the window ID; on failure `result.ok == false` and
`result.error` points to a static error string.

The created window starts **unmapped** (invisible); call `WM::mapWindow(id)` to
show it.

```cpp
WM::WindowCreateInfo info{};
info.title = "My Window";

auto r = WM::createWindow(info);
if (!r.ok) {
  // r.error describes the failure
  std::exit(1);
}
uint32_t window = r.unwrap();
WM::mapWindow(window);
```

### Mapping / visibility

- `void WM::mapWindow(uint32_t window)`
- `void WM::unmapWindow(uint32_t window)`

### Window operations

- `void WM::closeWindow(uint32_t window)`
- `void WM::resizeWindow(uint32_t window, uint32_t width, uint32_t height)`
- `void WM::moveWindow(uint32_t window, double x, double y)`
- `void WM::focusWindow(uint32_t window)`
- `void WM::minimizeWindow(uint32_t window)`
- `void WM::deminimizeWindow(uint32_t window)`
- `void WM::toggleFullscreenWindow(uint32_t window)`
- `void WM::setWindowMode(uint32_t window, WM::WindowMode mode)`
- `void WM::setWindowTitle(uint32_t window, const char* title)`
- `bool WM::isMinimized(uint32_t window)`
- `bool WM::isFullscreen(uint32_t window)`
- `WM::WindowInfo WM::getWindowInfo(uint32_t window)`

## Screens

### `void WM::getSystemsScreens(WM::Screen* buffer, uint32_t* screenCount)`
Copies available screens into `buffer` and writes the number of screens into
`screenCount`.

```cpp
WM::Screen screens[32]{};
uint32_t count = 0;
WM::getSystemsScreens(screens, &count);
```

## Event system

### `void WM::pollEvents()`
Polls OS events and fills the internal event queue. Call once per frame.

### `bool WM::getNextEvent(WM::Event* event)`
Reads the next event from the queue. Returns `false` when the queue is empty.

```cpp
WM::pollEvents();
WM::Event event{};
while (WM::getNextEvent(&event)) {
  // ...
}
```

### `enum WM::EventType`

Window:
- `WM::WindowClose`
- `WM::WindowResize` (`data.dimensions`)
- `WM::WindowFocus` (`data.focused`)
- `WM::WindowMinimize` (`data.minimized`)
- `WM::WindowFullScreen` (`data.fullscreen`)
- `WM::WindowMove` (`data.position`)

Keyboard:
- `WM::KeyDown` (`data.key`)
- `WM::KeyUp` (`data.key`)
- `WM::KeyRepeat` (`data.key`)
- `WM::KeyComb` (`data.combination`)

Mouse:
- `WM::MouseMove` (`data.mouseMoveDelta`)
- `WM::MouseDown` (`data.mouseButton`)
- `WM::MouseUp` (`data.mouseButton`)
- `WM::MouseScroll` (`data.mouseScrollDelta`)

### Key events

For `KeyDown/KeyUp/KeyRepeat`, use:

- `event.data.key.keyCode`
- `event.data.key.osLayoutKey` (UTF-8 bytes from OS layout, up to 7 chars + NUL)

For `KeyComb` (key + modifiers), use:

- `event.data.combination.press.keyCode`
- `event.data.combination.press.osLayoutKey`
- `event.data.combination.modifierCombination` (`WM::ModifierBits` mask)

### Modifier bits

```cpp
if (event.eventType == WM::KeyComb) {
  uint32_t key = event.data.combination.press.keyCode;
  uint32_t mods = event.data.combination.modifierCombination;

  if (key == KEY_S && (mods & WM::MOD_SUPER)) {
    // Super+S (Command on macOS)
  }
}
```

## Input state helpers

- `bool WM::isKeyPressed(uint8_t key)`
- `WM::vec2<double> WM::getMousePosition()`
- `WM::vec2<double> WM::getMousePositionDelta()`
- `bool WM::isMouseLeftDown()`
- `bool WM::isMouseRightDown()`
- `bool WM::isMouseMiddleDown()`

## Cursor control

- `void WM::setCursorVisible(bool visible)`
- `void WM::setCursorLook(WM::CursorType type)`
- `void WM::pushCursorLook(WM::CursorType type)` (stack-based; do not mix with `setCursorLook`)
- `void WM::popCursorLook()`
- `void WM::lockCursor()`
- `void WM::unlockCursor()`

## Callbacks

All callback setters take a window ID and a function pointer.

### Close callback

```cpp
bool onClose(uint32_t window) {
  // return false to cancel close
  return true;
}

WM::setCloseCallback(window, onClose);
```

### Resize callback

```cpp
void onResize(uint32_t window, uint32_t width, uint32_t height) {
  // ...
}

WM::setResizeCallback(window, onResize);
```

Other callbacks:
- `WM::setMoveCallback`
- `WM::setFocusCallback`
- `WM::setLostFocusCallback`
- `WM::setMinimizeCallback`
- `WM::setRestoreCallback`
- `WM::setEnterFullscreenCallback`
- `WM::setExitFullscreenCallback`

## Platform-specific

### macOS

- `double WM::getBackingScaleFactor(uint32_t window)`
- `void* WM::createMetalLayer(uint32_t window, void* device)` (returns `CAMetalLayer*` as `void*`)

`createMetalLayer` is easiest to call from an Objective-C++ (`.mm`) translation unit.

### Linux (Vulkan helpers)

The header references Vulkan types under `PLATFORM_LINUX`. Include Vulkan before
including `wm/window-manager.h`:

```cpp
#include <vulkan/vulkan.h>
#include <wm/window-manager.h>
```

Then:

- `void WM::getRequiredExtensions(const char*** data, uint32_t* count)`
- `void WM::createVkSurface(uint32_t window, VkInstance instance, VkSurfaceKHR* surface)`
