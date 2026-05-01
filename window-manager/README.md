# window-manager

Minimal C++20 windowing abstraction with a small event queue and input helpers.

- macOS: AppKit backend (`src/mac-os-implementation/`)
- Linux: XCB backend (`src/linux-implementation/`) (work in progress)

## Build (CMake)

```sh
cmake -S . -B build
cmake --build build
```

This produces a static library target named `window-manager`.

## Use

If you add this repo via CMake (`add_subdirectory(...)`), link `window-manager`
and include:

```cpp
#include <wm/window-manager.h>

int main() {
  WM::init();

  WM::WindowCreateInfo info{};
  info.title = "Hello";

  uint32_t window = WM::createWindow(info).unwrap();
  WM::mapWindow(window); // show the window

  while (!WM::shouldClose()) {
    WM::pollEvents();

    WM::Event event;
    while (WM::getNextEvent(&event)) {
      // handle events
    }
  }

  WM::quit();
}
```

## Docs

- `docs/windowmgr-readme.md`
- `docs/windowmgr-api-ref.md`
- `docs/windowmgr-examples.md`
