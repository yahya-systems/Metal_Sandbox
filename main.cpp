#include "core/log.hpp"
#include "gal/gal.hpp"
#include "window-manager/src/mac-os-implementation/keycodes.h"
#include <unistd.h>
#define WM_INCLUDE_METAL
#include "wm/window-manager.h"
#include <cstdlib>
#include <stdio.h>
#include <time.h> // For clock_gettime

struct FrameStats {
  double lastTime = 0;
  double accumulatedTime = 0;
  uint32_t frameCount = 0;

  // Helper to get time in seconds with high precision
  double getTime() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
  }

  void update() {
    double currentTime = getTime();
    if (lastTime == 0) {
      lastTime = currentTime;
      return;
    }

    double deltaTime = currentTime - lastTime;
    lastTime = currentTime;

    accumulatedTime += deltaTime;
    frameCount++;

    // Emit stats every second
    if (accumulatedTime >= 1.0) {
      double avgFps = (double)frameCount / accumulatedTime;
      double avgMs = (accumulatedTime / frameCount) * 1000.0;

      printf("Perf: %.2f FPS | %.3f ms\n", avgFps, avgMs);

      accumulatedTime = 0;
      frameCount = 0;
    }
  }
};

void resizeCallback(uint32_t window, uint32_t width, uint32_t height) {
  gal::updateLayerSize(width, height);
}

bool wireFrame = false;

int main() {
  WM::init();

  WM::WindowCreateInfo info{};
  info.title = "Engineering-Grade Spin"; // It definitely works now

  uint32_t window = WM::createWindow(info).unwrap();
  WM::mapWindow(window);
  WM::setResizeCallback(window, resizeCallback);

  gal::init();

  gal::getMetalLayer(WM::createMetalLayer(window, gal::getMetalDevice()));

  FrameStats stats;
  uint32_t resolution = 1;

  WM::lockCursor();

  bool moving = true;

  while (!WM::shouldClose()) {
    stats.update(); // Track timing before/after the frame
    WM::pollEvents();

    WM::Event ev;
    while (WM::getNextEvent(&ev)) {
      switch (ev.eventType) {
      case WM::KeyDown: {
        if (ev.data.key.keyCode == KEY_ESCAPE) {
          WM::unlockCursor();
          moving = false;
        }
        if (ev.data.key.keyCode == KEY_K) {
          wireFrame = !wireFrame;
        }
        if (ev.data.key.keyCode == KEY_R) {
          resolution++;
          gal::updateTerrain(resolution);
        }
        if (ev.data.key.keyCode == KEY_Q) {
          WM::closeWindow(window);
        }
        if (ev.data.key.keyCode == KEY_F) {
          printf("enter a normal map value : ");
          float nmStrength;
          scanf("%f", &nmStrength);
          *gal::normalStrength = nmStrength;
        }
      }
      case WM::MouseDown: {
        if (ev.data.mouseButton == 0) {
          WM::lockCursor();
          moving = true;
        }
      }
      default:
        break;
      }
    }

    *gal::terrainScale +=
        0.1f * (WM::isKeyPressed(KEY_P) - WM::isKeyPressed(KEY_O));

    *gal::normalStrength += 0.1f * (WM::isKeyPressed(KEY_UP_ARROW) -
                                    WM::isKeyPressed(KEY_DOWN_ARROW));

    auto delta = WM::getMousePositionDelta();

    gal::run(wireFrame, -delta.y, -delta.x,
             WM::isKeyPressed(KEY_W) - WM::isKeyPressed(KEY_S),
             WM::isKeyPressed(KEY_D) - WM::isKeyPressed(KEY_A),
             WM::isKeyPressed(KEY_CONTROL) - WM::isKeyPressed(KEY_SPACE));
  }

  gal::cleanup();
  WM::quit();
}
