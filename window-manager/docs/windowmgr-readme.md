# window-manager (WM)

Small C++20 window + input abstraction with a fixed-size event queue.

- Public header: `include/wm/window-manager.h` (use `#include <wm/window-manager.h>`)
- Namespace: `WM::`
- Backends: macOS (AppKit), Linux (XCB, work in progress)

## Quick start

```cpp
#include <wm/window-manager.h>
#include <cstdio>

int main() {
  WM::init();

  WM::WindowCreateInfo createInfo{};
  createInfo.title = "My Window";
  createInfo.width = 1280;
  createInfo.height = 720;
  createInfo.styles = WM::StyleResizable | WM::StyleClosable | WM::StyleTitled;

  auto windowResult = WM::createWindow(createInfo);
  if (!windowResult.ok) {
    std::fprintf(stderr, "createWindow failed: %s\n", windowResult.error);
    return 1;
  }

  uint32_t window = windowResult.unwrap();
  WM::mapWindow(window); // createWindow starts unmapped

  while (!WM::shouldClose()) {
    WM::pollEvents();

    WM::Event event{};
    while (WM::getNextEvent(&event)) {
      // handle events
    }
  }

  WM::quit();
  return 0;
}
```

## Build (CMake)

```sh
cmake -S . -B build
cmake --build build
```

## Documentation

- `docs/windowmgr-api-ref.md`
- `docs/windowmgr-examples.md`

## Notes

- `WM::createWindow(...)` returns `Result<uint32_t>` (see `src/Result.hpp`).
- Modifier bits for `WM::KeyComb` events are `WM::MOD_SHIFT`, `WM::MOD_CTRL`,
  `WM::MOD_ALT`, `WM::MOD_SUPER` (Command on macOS).
