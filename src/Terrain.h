#pragma once
#include "Resources/buffer.h"
#include "core/containers.hpp"
#include <Metal/Metal.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

using namespace gal;

struct Vertex {
  float x, y, z, u, v;
};

struct Tile {
  Vertex vertices[32][32];
};

class Terrain {
  uint32_t width;
  uint32_t height;
  uint32_t resolution;
  uint32_t vertexCount;
  uint32_t indexCount;
  id<MTLDevice> metalDevice;
  id<MTLBuffer> vertexBuffer;
  id<MTLBuffer> indexBuffer;

public:
  Terrain() {
    width = 0;
    height = 0;
    resolution = 0;
    vertexCount = 0;
    indexCount = 0;
    metalDevice = nil;
    vertexBuffer = nil;
    indexBuffer = nil;
  }
  Terrain(id<MTLDevice> mtlDevice, uint32_t w, uint32_t h, uint32_t res,
          bool *success)
      : width(w), height(h), resolution(res), metalDevice(mtlDevice) {
    vertexBuffer = nil;
    indexBuffer = nil;
    *success = updateTerrain(width, height, resolution);
  }

  // Terrain(Terrain &&terrain) {
  //   width = terrain.width;
  //   height = terrain.height;
  //   resolution = terrain.resolution;
  //   vertexCount = terrain.vertexCount;
  //   indexCount = terrain.indexCount;
  //   metalDevice = terrain.metalDevice;
  //   vertexBuffer = terrain.vertexBuffer;
  //   indexBuffer = terrain.indexBuffer;
  //   terrain.width = 0;
  //   terrain.height = 0;
  //   terrain.resolution = 0;
  //   terrain.vertexCount = 0;
  //   terrain.indexCount = 0;
  //   terrain.metalDevice = nil;
  //   terrain.vertexBuffer = nil;
  //   terrain.indexBuffer = nil;
  // }

  bool updateTerrain(uint32_t w, uint32_t h, uint32_t res) {
    if (vertexBuffer != nil) {
      [vertexBuffer release];
    }
    if (indexBuffer != nil) {
      [indexBuffer release];
    }
    if (res == 0 || w == 0 || h == 0) {
      return false;
    }
    width = w;
    height = h;
    resolution = res;

    w++;
    h++;

    uint32_t cols = w * res;
    uint32_t rows = h * res;

    vertexCount = cols * rows;

    size_t vertexBufferSize = cols * rows * 5 * sizeof(float);
    size_t indexBufferSize = (cols - 1) * (rows - 1) * 6 * sizeof(uint32_t);

    BufferCreateInfo vertexStagingBufferCreateInfo{};
    vertexStagingBufferCreateInfo.device = metalDevice;
    vertexStagingBufferCreateInfo.storageMode = MODE_SHARED;
    vertexStagingBufferCreateInfo.length = vertexBufferSize;
    auto result = createBuffer(vertexStagingBufferCreateInfo);
    if (!result.ok) {
      return false;
    }

    id<MTLBuffer> vertexStagingBuffer = result.unwrap();
    float *vertices = (float *)mapBuffer(vertexStagingBuffer).unwrap();

    BufferCreateInfo indexStagingBufferCreateInfo =
        vertexStagingBufferCreateInfo;
    indexStagingBufferCreateInfo.length = indexBufferSize;
    result = createBuffer(indexStagingBufferCreateInfo);
    if (!result.ok) {
      [vertexStagingBuffer release];
      return false;
    }

    id<MTLBuffer> indexStagingBuffer = result.unwrap();
    uint32_t *indices = (uint32_t *)mapBuffer(indexStagingBuffer).unwrap();

    uint32_t k = 0;
    for (uint32_t i = 0; i < rows; i++) {
      for (uint32_t j = 0; j < cols; j++) {
        vertices[i * cols * 5 + j * 5] = (float)j / res;
        vertices[i * cols * 5 + j * 5 + 1] = 0;
        vertices[i * cols * 5 + j * 5 + 2] = (float)i / res;
        vertices[i * cols * 5 + j * 5 + 3] = (float)j / (cols - 1);
        vertices[i * cols * 5 + j * 5 + 4] = (float)i / (rows - 1);

        if (i != rows - 1 && j != cols - 1) {
          indices[k++] = j + i * cols;
          indices[k++] = j + (i + 1) * cols;
          indices[k++] = (j + 1) + i * cols;
          indices[k++] = (j + 1) + i * cols;
          indices[k++] = j + (i + 1) * cols;
          indices[k++] = (j + 1) + (i + 1) * cols;
        }
      }
    }

    indexCount = k;

    BufferCreateInfo vertexBufferCreateInfo{};
    vertexBufferCreateInfo.device = metalDevice;
    vertexBufferCreateInfo.storageMode = MODE_PRIVATE;
    vertexBufferCreateInfo.length = vertexBufferSize;
    result = createBuffer(vertexBufferCreateInfo);
    if (!result.ok) {
      [vertexStagingBuffer release];
      [indexStagingBuffer release];
      return false;
    }
    vertexBuffer = result.unwrap();

    copyDataToPrivateBuffer(vertexBuffer, 0, vertexStagingBuffer, 0,
                            vertexBufferSize, metalDevice);

    [vertexStagingBuffer release];

    BufferCreateInfo indexBufferCreateInfo = vertexBufferCreateInfo;
    indexBufferCreateInfo.length = indexBufferSize;
    result = createBuffer(indexBufferCreateInfo);
    if (!result.ok) {
      [vertexBuffer release];
      [indexStagingBuffer release];
      return false;
    }
    indexBuffer = result.unwrap();

    copyDataToPrivateBuffer(indexBuffer, 0, indexStagingBuffer, 0,
                            indexBufferSize, metalDevice);

    [indexStagingBuffer release];
    return true;
  }

  id<MTLBuffer> getVertexBuffer() const { return vertexBuffer; };
  id<MTLBuffer> getIndexBuffer() const { return indexBuffer; };

  uint32_t getVertexCount() const { return vertexCount; };
  uint32_t getIndexCount() const { return indexCount; };
  uint32_t getWidth() const { return width; };
  uint32_t getHeight() const { return height; };

  void cleanup() {
    if (vertexBuffer)
      [vertexBuffer release];
    if (indexBuffer)
      [indexBuffer release];
  }
};

inline void createTerrain(uint32_t width, uint32_t height, uint32_t resolution,
                          Vertex **vertexBuffer, uint32_t **indexBuffer,
                          uint32_t *vertexBufferSize,
                          uint32_t *indexBufferSize) {
  uint32_t cols = width * resolution;
  uint32_t rows = height * resolution;

  auto get_idx = [&](int i, int j) { return j + i * cols; };

  *vertexBufferSize = cols * rows * sizeof(Vertex);
  Vertex *vertices = (Vertex *)malloc(*vertexBufferSize);
  if (!vertices) {
    *vertexBuffer = nullptr;
    *indexBuffer = nullptr;
  }

  for (uint32_t y = 0; y < rows; y++) {
    for (uint32_t x = 0; x < cols; x++) {
      float u = (float)x / (cols - 1);
      float v = (float)y / (rows - 1);

      vertices[get_idx(y, x)] =
          Vertex{(float)x / resolution, 0.0f, (float)y / resolution, u, v};
    }
  }

  *indexBufferSize = (cols - 1) * (rows - 1) * 6 * sizeof(uint32_t);
  uint32_t *indices = (uint32_t *)malloc(*indexBufferSize);

  if (!indices) {
    *vertexBuffer = nullptr;
    *indexBuffer = nullptr;
  }

  uint32_t index{};

  for (uint32_t y = 0; y < rows - 1; y++) {
    for (uint32_t x = 0; x < cols - 1; x++) {
      indices[index++] = get_idx(y, x);
      indices[index++] = get_idx(y + 1, x);
      indices[index++] = get_idx(y, x + 1);

      indices[index++] = get_idx(y, x + 1);
      indices[index++] = get_idx(y + 1, x);
      indices[index++] = get_idx(y + 1, x + 1);
    }
  }

  *indexBuffer = indices;
  *vertexBuffer = vertices;
}
