#pragma once
#include <cstdint>

namespace gal {
void init();
void updateCamera(float pitch, float yaw, int forward, int right, int up);
void run(bool wireframe, float pitch, float yaw, int forward, int right,
         int up);
void getMetalLayer(void *metalLayer);
void updateLayerSize(uint32_t width, uint32_t height);
void updateTerrain(uint32_t resolution);
void *getMetalDevice();
void cleanup();
inline float *terrainScale;
inline float *normalStrength;
} // namespace gal
