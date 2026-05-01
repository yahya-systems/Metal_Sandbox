# WM Examples

Common usage patterns using the current public API (`WM::`).

## Basic window

```cpp
#include <wm/window-manager.h>
#include <cstdio>
#include <unistd.h>

int main() {
  WM::init();

  WM::WindowCreateInfo createInfo{};
  createInfo.title = "Hello Window";
  createInfo.width = 800;
  createInfo.height = 600;
  createInfo.styles = WM::StyleResizable | WM::StyleClosable | WM::StyleTitled;

  auto r = WM::createWindow(createInfo);
  if (!r.ok) {
    std::fprintf(stderr, "createWindow failed: %s\n", r.error);
    return 1;
  }
  uint32_t window = r.unwrap();
  WM::mapWindow(window);

  while (!WM::shouldClose()) {
    WM::pollEvents();

    // your rendering here
    usleep(16000); // ~60 FPS
  }

  WM::quit();
  return 0;
}
```

## Handling keyboard input

### Event-based (edge-triggered)

```cpp
WM::Event event{};
while (WM::getNextEvent(&event)) {
  if (event.eventType == WM::KeyDown) {
    switch (event.data.key.keyCode) {
    case KEY_ESCAPE:
      std::puts("Escape pressed!");
      break;
    case KEY_SPACE:
      std::puts("Space pressed!");
      break;
    case KEY_W:
      std::puts("W pressed!");
      break;
    }
  }
}
```

### State-based (continuous)

```cpp
// good for camera movement
while (!WM::shouldClose()) {
  WM::pollEvents();

  float speed = 0.1f;
  if (WM::isKeyPressed(KEY_W)) camera.moveForward(speed);
  if (WM::isKeyPressed(KEY_S)) camera.moveBackward(speed);
  if (WM::isKeyPressed(KEY_A)) camera.moveLeft(speed);
  if (WM::isKeyPressed(KEY_D)) camera.moveRight(speed);
}
```

## Keyboard shortcuts

`WM::KeyComb` events provide a key press plus a modifier mask (`WM::MOD_SHIFT`,
`WM::MOD_CTRL`, `WM::MOD_ALT`, `WM::MOD_SUPER`).

```cpp
WM::Event event{};
while (WM::getNextEvent(&event)) {
  if (event.eventType != WM::KeyComb)
    continue;

  uint32_t key = event.data.combination.press.keyCode;
  uint32_t mods = event.data.combination.modifierCombination;

  // Super+S (Command on macOS)
  if (key == KEY_S && mods == WM::MOD_SUPER) save();

  // Super+Shift+S
  if (key == KEY_S && mods == (WM::MOD_SUPER | WM::MOD_SHIFT)) saveAs();

  // Ctrl+Z
  if (key == KEY_Z && mods == WM::MOD_CTRL) undo();

  // Ctrl+Shift+Z
  if (key == KEY_Z && mods == (WM::MOD_CTRL | WM::MOD_SHIFT)) redo();
}
```

## Mouse input

### FPS-style camera toggle

```cpp
bool fpsMode = false;

while (!WM::shouldClose()) {
  WM::pollEvents();

  WM::Event event{};
  while (WM::getNextEvent(&event)) {
    switch (event.eventType) {
    case WM::KeyDown:
      if (event.data.key.keyCode == KEY_ESCAPE) {
        fpsMode = !fpsMode;
        if (fpsMode)
          WM::lockCursor();
        else
          WM::unlockCursor();
      }
      break;

    case WM::MouseMove:
      if (fpsMode) {
        float sensitivity = 0.002f;
        camera.yaw += (float)event.data.mouseMoveDelta.x * sensitivity;
        camera.pitch -= (float)event.data.mouseMoveDelta.y * sensitivity;
      }
      break;

    default:
      break;
    }
  }
}
```

### Click detection

```cpp
WM::Event event{};
while (WM::getNextEvent(&event)) {
  if (event.eventType != WM::MouseDown)
    continue;

  auto pos = WM::getMousePosition();
  if (event.data.mouseButton == 0) {
    std::printf("Left click at (%.1f, %.1f)\n", pos.x, pos.y);
  } else if (event.data.mouseButton == 1) {
    std::printf("Right click at (%.1f, %.1f)\n", pos.x, pos.y);
  }
}
```

### Dragging

```cpp
bool isDragging = false;
WM::vec2<double> dragStart{};

WM::Event event{};
while (WM::getNextEvent(&event)) {
  switch (event.eventType) {
  case WM::MouseDown:
    if (event.data.mouseButton == 0) {
      isDragging = true;
      dragStart = WM::getMousePosition();
    }
    break;

  case WM::MouseUp:
    if (event.data.mouseButton == 0) {
      isDragging = false;
    }
    break;

  case WM::MouseMove:
    if (isDragging) {
      auto current = WM::getMousePosition();
      double deltaX = current.x - dragStart.x;
      double deltaY = current.y - dragStart.y;
      object.x += (float)deltaX;
      object.y += (float)deltaY;
      dragStart = current;
    }
    break;

  default:
    break;
  }
}
```

### Mouse wheel zoom

```cpp
WM::Event event{};
while (WM::getNextEvent(&event)) {
  if (event.eventType != WM::MouseScroll)
    continue;

  float zoomSpeed = 0.1f;
  camera.zoom += (float)event.data.mouseScrollDelta.y * zoomSpeed;
  if (camera.zoom < 0.5f) camera.zoom = 0.5f;
  if (camera.zoom > 5.0f) camera.zoom = 5.0f;
}
```

## Cursor management

```cpp
WM::setCursorLook(WM::CURSOR_IBEAM);
WM::setCursorLook(WM::CURSOR_POINTING_HAND);
WM::setCursorLook(WM::CURSOR_CROSSHAIR);
WM::setCursorLook(WM::CURSOR_ARROW);
```

Cursor stack (nested UI):

```cpp
WM::pushCursorLook(WM::CURSOR_POINTING_HAND);
WM::pushCursorLook(WM::CURSOR_OPEN_HAND);
WM::pushCursorLook(WM::CURSOR_CLOSED_HAND);
WM::popCursorLook();
WM::popCursorLook();
WM::popCursorLook();
```

## Window callbacks

### Resize callback

```cpp
void onResize(uint32_t window, uint32_t width, uint32_t height) {
  std::printf("Window %u resized to %ux%u\n", window, width, height);
  recreateFramebuffer(width, height);
}

int main() {
  WM::init();
  WM::WindowCreateInfo createInfo{};

  auto r = WM::createWindow(createInfo);
  if (!r.ok) return 1;
  uint32_t window = r.unwrap();
  WM::mapWindow(window);

  WM::setResizeCallback(window, onResize);
  // ...
}
```

### Pause when unfocused

```cpp
bool isPaused = false;

void onFocus(uint32_t) { isPaused = false; }
void onLostFocus(uint32_t) { isPaused = true; }

// ...
WM::setFocusCallback(window, onFocus);
WM::setLostFocusCallback(window, onLostFocus);
```

## Multi-window application

```cpp
WM::WindowCreateInfo mainInfo{};
mainInfo.title = "Main Window";
auto rMain = WM::createWindow(mainInfo);
if (!rMain.ok) std::exit(1);
uint32_t mainWindow = rMain.unwrap();
WM::mapWindow(mainWindow);

WM::WindowCreateInfo toolInfo{};
toolInfo.title = "Tools";
auto rTool = WM::createWindow(toolInfo);
if (!rTool.ok) std::exit(1);
uint32_t toolWindow = rTool.unwrap();
WM::mapWindow(toolWindow);

while (!WM::shouldClose()) {
  WM::pollEvents();

  WM::Event event{};
  while (WM::getNextEvent(&event)) {
    if (event.windowID == mainWindow) handleMainWindow(event);
    if (event.windowID == toolWindow) handleToolWindow(event);
  }
}
```

## HiDPI / Retina (macOS)

```cpp
double scale = WM::getBackingScaleFactor(window);
uint32_t pixelWidth = (uint32_t)(logicalWidth * scale);
uint32_t pixelHeight = (uint32_t)(logicalHeight * scale);
```

## Integration with rendering APIs

### Metal layer (macOS)

`WM::createMetalLayer` returns a `CAMetalLayer*` as `void*` and attaches it to
the window's content view. This is easiest to call from Objective-C++ (`.mm`).

```cpp
#include <wm/window-manager.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

id<MTLDevice> device = MTLCreateSystemDefaultDevice();
CAMetalLayer* layer =
    (CAMetalLayer*)WM::createMetalLayer(window, (void*)device);
```

### Vulkan surface (Linux)

```cpp
#include <vulkan/vulkan.h>
#include <wm/window-manager.h>

const char** extensions = nullptr;
uint32_t extensionCount = 0;
WM::getRequiredExtensions(&extensions, &extensionCount);

VkSurfaceKHR surface{};
WM::createVkSurface(window, instance, &surface);
```

## Complete game loop sketch

```cpp
#include <wm/window-manager.h>
#include <unistd.h>

struct Camera {
  float x, y, z;
  float yaw, pitch;
} camera{0, 0, 5, 0, 0};

bool fpsMode = false;

int main() {
  WM::init();

  WM::WindowCreateInfo createInfo{};
  createInfo.title = "3D Viewer";
  createInfo.width = 1280;
  createInfo.height = 720;
  createInfo.styles = WM::StyleResizable | WM::StyleClosable | WM::StyleTitled;

  auto r = WM::createWindow(createInfo);
  if (!r.ok) return 1;
  uint32_t window = r.unwrap();
  WM::mapWindow(window);

  while (!WM::shouldClose()) {
    WM::pollEvents();

    WM::Event event{};
    while (WM::getNextEvent(&event)) {
      switch (event.eventType) {
      case WM::KeyDown:
        if (event.data.key.keyCode == KEY_ESCAPE) {
          fpsMode = !fpsMode;
          fpsMode ? WM::lockCursor() : WM::unlockCursor();
        }
        break;

      case WM::MouseMove:
        if (fpsMode) {
          camera.yaw += (float)event.data.mouseMoveDelta.x * 0.002f;
          camera.pitch -= (float)event.data.mouseMoveDelta.y * 0.002f;
        }
        break;

      case WM::WindowResize:
        recreateFramebuffer((uint32_t)event.data.dimensions.x,
                            (uint32_t)event.data.dimensions.y);
        break;

      default:
        break;
      }
    }

    float speed = 0.1f;
    if (WM::isKeyPressed(KEY_W)) camera.z -= speed;
    if (WM::isKeyPressed(KEY_S)) camera.z += speed;
    if (WM::isKeyPressed(KEY_A)) camera.x -= speed;
    if (WM::isKeyPressed(KEY_D)) camera.x += speed;

    usleep(16000);
  }

  WM::quit();
  return 0;
}
```
